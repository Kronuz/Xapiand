// Probes of harness/light_share_task.patch (the heavy share in asio_scheduler::drain()).
// `share_probe NAME`; LS_HEAVY_SHARE is read once per process, so the runner sets it per scenario.
//   cap      8 loops, share 3: heavy turns from other threads, light demoted mixed in; the share is
//            never exceeded, every handle runs, and afterwards exactly `share` turns can overlap (no leak).
//   parked   share 1: a light demoted handle reaching a loop parked for a slot; does it run while the
//            slot is held elsewhere?
//   bypass   share 4, one loop, slots free: a heavy handle queued ahead of a self-reposting light one;
//            how many light turns run first? (at most 1)
//   heavyonly share 2, 4 loops, no light work: all 4 heavy turns run at once (the share binds only while light waits)
//   gate     share 2, 4 loops, a light handle on each first: exactly 2 heavy turns overlap
// parked and cap run with LS_LIGHT_WINDOW_MS=0 (the share always binds); gate with a long window.
#include "task_asio.hh"

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using clk = std::chrono::steady_clock;

static int failures = 0;
#define CHECK(cond) \
	do { \
		if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); ++failures; } \
	} while (0)

namespace {

struct co {
	struct promise_type {
		co get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
		std::suspend_always initial_suspend() noexcept { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }
		void return_void() noexcept {}
		void unhandled_exception() noexcept { std::terminate(); }
	};
	std::coroutine_handle<promise_type> h;
};

struct repost_demoted {
	task::asio_scheduler& s;
	bool await_ready() const noexcept { return false; }
	void await_suspend(std::coroutine_handle<> h) { s.post_demoted(h); }
	void await_resume() const noexcept {}
};

void spin(std::chrono::microseconds d) {
	const auto end = clk::now() + d;
	while (clk::now() < end) {}
}

// N loops, one thread each; torn down in task_asio.hh's order.
struct loops {
	std::vector<std::unique_ptr<asio::io_context>> io;
	std::vector<std::unique_ptr<task::asio_scheduler>> s;
	std::vector<asio::executor_work_guard<asio::io_context::executor_type>> guards;
	std::vector<std::thread> t;
	std::vector<std::coroutine_handle<>> frames;
	std::mutex fm;
	explicit loops(int n) {
		for (int i = 0; i < n; ++i) {
			io.push_back(std::make_unique<asio::io_context>(1));
			s.push_back(std::make_unique<task::asio_scheduler>(io.back()->get_executor()));
			guards.push_back(asio::make_work_guard(*io.back()));
		}
		for (int i = 0; i < n; ++i) { t.emplace_back([this, i] { io[i]->run(); }); }
	}
	void keep(std::coroutine_handle<> h) { std::lock_guard<std::mutex> lk(fm); frames.push_back(h); }
	~loops() {
		for (auto& g : guards) { g.reset(); }
		for (auto& c : io) { c->stop(); }
		for (auto& th : t) { th.join(); }
		for (auto& x : s) { x->close(); }
		for (auto& x : s) { x->destroy_roots(); }
		s.clear();
		for (auto h : frames) { h.destroy(); }
		io.clear();
	}
};

bool wait_until(const std::atomic<int>& v, int want, std::chrono::milliseconds limit) {
	const auto end = clk::now() + limit;
	while (v.load() < want) {
		if (clk::now() > end) { return false; }
		std::this_thread::sleep_for(1ms);
	}
	return true;
}

// ---- cap
std::atomic<int> inside{0}, max_inside{0}, done{0}, light_done{0};

co heavy_turn(std::chrono::microseconds d) {
	const int now = ++inside;
	for (int m = max_inside.load(); now > m && !max_inside.compare_exchange_weak(m, now);) {}
	spin(d);
	--inside;
	++done;
	co_return;
}
co light_turn() {
	++light_done;
	co_return;
}

std::atomic<int> overlap{0}, overlap_max{0}, overlap_done{0};
co overlap_turn(int want) {
	const int now = ++overlap;
	for (int m = overlap_max.load(); now > m && !overlap_max.compare_exchange_weak(m, now);) {}
	const auto end = clk::now() + 400ms;
	while (overlap.load() < want && clk::now() < end) { std::this_thread::yield(); }
	--overlap;
	++overlap_done;
	co_return;
}

void test_cap(int share) {
	constexpr int n = 8, per = 400;
	loops L(n);
	std::vector<std::thread> producers;
	for (int p = 0; p < n; ++p) {
		producers.emplace_back([&, p] {
			for (int k = 0; k < per; ++k) {
				for (int i = (p + k) % n, j = 0; j < 2; ++j, i = (i + 3) % n) {
					auto h = heavy_turn(20us).h;
					L.keep(h);
					L.s[i]->post_heavy(h);
					if (k % 3 == 0) {
						auto l = light_turn().h;
						L.keep(l);
						L.s[i]->post_demoted(l);
					}
				}
			}
		});
	}
	for (auto& th : producers) { th.join(); }
	const int heavy_total = n * per * 2;
	CHECK(wait_until(done, heavy_total, 120s));
	CHECK(wait_until(light_done, n * ((per + 2) / 3) * 2, 30s));
	std::printf("cap: share %d, heavy %d/%d, light %d, max concurrent heavy %d\n", share, done.load(), heavy_total,
		light_done.load(), max_inside.load());
	CHECK(max_inside.load() <= share);
	// No leak: share+1 turns on share+1 loops, each waiting for share+1 to overlap; exactly `share` can.
	for (int i = 0; i <= share; ++i) {
		auto h = overlap_turn(share + 1).h;
		L.keep(h);
		L.s[i]->post_heavy(h);
	}
	CHECK(wait_until(overlap_done, share + 1, 30s));
	std::printf("cap: after the stress, %d of %d heavy turns overlapped (want %d)\n", overlap_max.load(), share + 1, share);
	CHECK(overlap_max.load() == share);
}

// ---- parked
std::atomic<bool> release_a{false};
std::atomic<int> a_in{0}, b_heavy{0}, b_light{0};
std::atomic<long long> b_light_at{0}, a_released_at{0};

co holder() {
	++a_in;
	const auto end = clk::now() + 5s;
	while (!release_a.load() && clk::now() < end) { std::this_thread::sleep_for(100us); }
	co_return;
}
co b_heavy_turn() {
	++b_heavy;
	co_return;
}
co b_light_turn() {
	b_light_at = clk::now().time_since_epoch().count();
	++b_light;
	co_return;
}

void test_parked() {
	loops L(2);
	auto a = holder().h;
	L.keep(a);
	L.s[0]->post_heavy(a);
	CHECK(wait_until(a_in, 1, 5s));
	auto hb = b_heavy_turn().h;
	L.keep(hb);
	L.s[1]->post_heavy(hb);
	std::this_thread::sleep_for(50ms);  // B's drain runs: with share 1 it parks
	auto lb = b_light_turn().h;
	L.keep(lb);
	L.s[1]->post_demoted(lb);
	std::this_thread::sleep_for(300ms);
	const bool ran_while_held = b_light.load() == 1;
	a_released_at = clk::now().time_since_epoch().count();
	release_a = true;
	CHECK(wait_until(b_light, 1, 5s));
	const double after_ms = b_light.load() ? double(b_light_at.load() - a_released_at.load()) / 1e6 : -1;
	std::printf("parked: light demoted handle on the parked loop ran while the slot was held elsewhere: %s; "
		"heavy on B ran: %d; light ran %.2f ms after the holder was released\n",
		ran_while_held ? "yes" : "NO", b_heavy.load(), ran_while_held ? 0.0 : after_ms);
	if (!ran_while_held) { ++failures; }  // the design says light handles never wait for a slot
}

// ---- bypass
std::atomic<int> light_turns{0}, heavy_saw{-1};
co heavy_once() {
	heavy_saw = light_turns.load();
	co_return;
}
co light_stream(task::asio_scheduler& s, int k) {
	for (int i = 0; i < k; ++i) {
		++light_turns;
		co_await repost_demoted{s};
	}
}

void test_bypass() {
	constexpr int k = 1000;
	loops L(1);
	auto& s = *L.s[0];
	auto h = heavy_once().h;
	auto l = light_stream(s, k).h;
	L.keep(h);
	L.keep(l);
	asio::post(*L.io[0], [&s, h, l] {
		s.post_heavy(h);
		s.post_demoted(l);
	});
	CHECK(wait_until(light_turns, k, 10s));
	std::this_thread::sleep_for(50ms);
	std::printf("bypass: the heavy handle queued first ran after %d of %d light turns (FIFO: 0)\n", heavy_saw.load(), k);
	CHECK(heavy_saw.load() >= 0);
	if (heavy_saw.load() > 1) { ++failures; }  // free slots, yet the heavy head waited for the whole light stream
}

// ---- heavyonly / gate
void test_overlap(bool light, int share) {
	constexpr int n = 4;
	loops L(n);
	if (light) {
		for (int i = 0; i < n; ++i) {
			auto l = light_turn().h;
			L.keep(l);
			L.s[i]->post_demoted(l);
		}
		CHECK(wait_until(light_done, n, 5s));
	}
	for (int i = 0; i < n; ++i) {
		auto h = overlap_turn(n).h;
		L.keep(h);
		L.s[i]->post_heavy(h);
	}
	CHECK(wait_until(overlap_done, n, 30s));
	const int want = light ? share : n;
	std::printf("%s: %d heavy turns overlapped (want %d; share %d)\n", light ? "gate" : "heavyonly", overlap_max.load(), want, share);
	CHECK(overlap_max.load() == want);
}

}  // namespace

int main(int argc, char** argv) {
	const char* v = std::getenv("LS_HEAVY_SHARE");
	const int share = v ? std::atoi(v) : 0;
	const char* name = argc > 1 ? argv[1] : "";
	if (!std::strcmp(name, "cap")) {
		test_cap(share > 0 ? share : 1 << 20);
	} else if (!std::strcmp(name, "parked")) {
		test_parked();
	} else if (!std::strcmp(name, "bypass")) {
		test_bypass();
	} else if (!std::strcmp(name, "heavyonly")) {
		test_overlap(false, share);
	} else if (!std::strcmp(name, "gate")) {
		test_overlap(true, share);
	} else {
		std::fprintf(stderr, "usage: share_probe cap|parked|bypass|heavyonly|gate\n");
		return 2;
	}
	std::printf("%s: %s (LS_HEAVY_SHARE=%s)\n", name, failures ? "FAIL" : "ok", v ? v : "unset");
	return failures ? 1 : 0;
}
