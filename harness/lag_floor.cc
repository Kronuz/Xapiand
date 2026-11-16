// lag_floor: the LagProbe's own floor on an otherwise idle io_context, with no Xapiand in the process.
// Runs reactor::LagProbe (interval from the first wake: idle 100 ms, or busy 1 ms when --busy keeps note_activity()
// going) beside a sleep-free spinner that re-posts itself and timestamps loop iterations, and prints the lateness
// and the iteration gap maxima per second.
//
//   c++ -std=c++20 -O2 -I<reactor-src> -I<asio>/include -DASIO_STANDALONE lag_floor.cc -o lag_floor -pthread
//   lag_floor [--busy] [--spin] [--burn N] [--secs S]       --burn N: N other threads spinning on CPU (host load)
#include "reactor_lag.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

using clk = std::chrono::steady_clock;

int main(int argc, char** argv) {
	bool busy = false, spin = false;
	int burn = 0, secs = 20;
	for (int i = 1; i < argc; ++i) {
		if (!std::strcmp(argv[i], "--busy")) busy = true;
		else if (!std::strcmp(argv[i], "--spin")) spin = true;
		else if (!std::strcmp(argv[i], "--burn")) burn = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "--secs")) secs = std::atoi(argv[++i]);
	}
	asio::io_context io;
	reactor::LagProbe probe(io);
	std::atomic<bool> stop{false};
	std::vector<std::thread> burners;
	for (int i = 0; i < burn; ++i) burners.emplace_back([&] { volatile unsigned long x = 0; while (!stop) x = x + 1; });

	// Spinner: each run re-posts itself; the gap between two runs is how long the loop spent elsewhere.
	std::uint64_t spin_max_us = 0, spin_runs = 0;
	auto last = clk::now();
	std::function<void()> again = [&] {
		const auto now = clk::now();
		const auto gap = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - last).count());
		if (gap > spin_max_us) spin_max_us = gap;
		++spin_runs;
		last = now;
		if (!stop) asio::post(io, again);
	};
	if (spin) asio::post(io, again);

	// --busy: a 5 ms ticker calling note_activity() keeps the probe at its 1 ms interval.
	asio::steady_timer ticker(io);
	std::function<void()> tick = [&] {
		probe.note_activity();
		ticker.expires_after(std::chrono::milliseconds(5));
		ticker.async_wait([&](const asio::error_code&) { if (!stop) tick(); });
	};
	if (busy) tick();

	probe.start();
	std::thread th([&] { io.run(); });
	reactor::LagSnapshot prev;
	for (int s = 1; s <= secs; ++s) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		auto snap = probe.snapshot();
		auto d = snap.since(prev);
		prev = snap;
		std::printf("t=%2ds wakes=%llu p50<=%lluus p99<=%lluus max_so_far=%lluus spin_max=%lluus runs=%llu\n", s,
			(unsigned long long)d.wakes, (unsigned long long)d.quantile_upper_us(0.5), (unsigned long long)d.quantile_upper_us(0.99),
			(unsigned long long)snap.max_us, (unsigned long long)spin_max_us, (unsigned long long)spin_runs);
		spin_max_us = 0;
		std::fflush(stdout);
	}
	stop = true;
	probe.stop();
	io.stop();
	th.join();
	for (auto& b : burners) b.join();
}
