/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Stored blobs and chunked uploads through a real lone-voter strata::group over POSIX files (cluster-release D12,
// D16; quest journal-replaces-wal A2, A3), with consensus's 1 MiB `command_bytes`:
//
// - A command past the limit is proposed as chunks and applies; proposed whole, consensus refuses it.
// - kill -9 at each crash point of a script of stored-blob writes, a chunked stored-blob write, a chunked upload
//   with a checkpoint racing it, a checkpoint, and an upload left open: a child process runs the script and
//   SIGKILLs itself at the point; the parent reopens the group. Every acknowledged blob reads back byte-equal, every
//   document present resolves to its bytes, the upload left open is retired by the reopened term, no file outlives
//   what names it, and recovery leaves each file the bundle names at its length (D16 step 2).
//
// The crash points are every chore step and journal IO completion (the journal's IO is held and completed one at a
// time), and in each entry's apply before its file effects, after them, and after its strata write.
// GROUP_CHUNK_FIRST and GROUP_CHUNK_STRIDE run a subset (harness/blob_crash_check.sh runs them all).

#include "test_harness.h"

#include <csignal>                 // for raise, SIGKILL
#include <array>
#include <cstdio>                  // for std::fprintf
#include <cstdlib>                 // for std::getenv
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>              // for waitpid
#include <unistd.h>                // for fork, pipe, _exit
#include <vector>

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "completion/seam.h"       // for HeldCompletion, InlineCompletion
#include "database/command.h"
#include "database/data.h"
#include "database/handler.h"
#include "database/shard_group.h"
#include "engine/document.h"
#include "engine/error.h"
#include "engine/strata/shard.h"   // for engine::strata::index_options
#include "journal/host.h"          // for kronuz::journal::Host
#include "strata/group/group.hh"

namespace {

namespace cs = kronuz::consensus;
namespace j = kronuz::journal;
namespace sg = strata::group;
namespace fs = std::filesystem;

constexpr cs::GroupId GID = 1;
constexpr std::uint64_t NEVER = ~std::uint64_t(0);

std::string pattern(std::size_t n, char seed) {
	std::string out(n, '\0');
	for (std::size_t i = 0; i < n; ++i) {
		out[i] = static_cast<char>(seed + static_cast<char>(i * 131 % 251));
	}
	return out;
}

// The crash point counter: at `crash_at` the process kills itself, as `kill -9` would.
std::uint64_t events = 0;
std::uint64_t crash_at = NEVER;

void point() {
	if (++events == crash_at) {
		std::raise(SIGKILL);
	}
}

// The shard group's application over the index the group opened, with crash points inside each entry's apply and
// a look at the file set as recovery left it, at the first entry applied after it.
struct App {
	using layout = prospect::search_layout;
	using state = ShardGroupState;

	std::function<const prospect::index&()> index;
	std::function<void(sg::file_set&)> recovered;
	std::optional<ShardApplication> app;
	bool first = true;

	ShardApplication& shard() {
		if (!app) {
			app.emplace(index());
		}
		return *app;
	}

	void effects(const state& s, const sg::entry& e, sg::file_set& f) {
		if (first) {
			first = false;
			if (recovered) {
				recovered(f);
			}
		}
		point();
		shard().effects(s, e, f);
		point();
	}
	std::optional<prospect::transaction> transaction(const state& s, const sg::entry& e) {
		return shard().transaction(s, e);
	}
	void advance(state& s, const sg::entry& e) {
		point();
		shard().advance(s, e);
	}
	std::string encode(const state& s) {
		return shard().encode(s);
	}
	state decode(std::string_view bytes) {
		return shard().decode(bytes);
	}
};

using group_type = sg::group<App>;

group_type::config config() {
	group_type::config c;
	c.id = GID;
	c.options.configuration.local = 1;
	c.options.configuration.voters = {1};
	c.options.configuration.cluster[0] = 'X';
	c.options.configuration.configuration[0] = 'G';
	c.options.admission = j::AdmissionLimits{{1ull << 30, 1u << 20}, {1u << 20, 16}, {96ull << 20, 64}, 64, 3};
	c.options.limits.command_bytes = COMMAND_ENTRY_BYTES;   // D12: the frame
	c.options.limits.rpc_bytes = 2 * COMMAND_ENTRY_BYTES;
	c.options.limits.log_bytes = 16 * COMMAND_ENTRY_BYTES;
	c.options.timing = cs::Timing{20, 40, 100, 200, 60000};
	c.identity = j::Identity{'X', 'S', 'H'};
	return c;
}

const std::array<char, 16> UUID{'c', 'h', 'u', 'n', 'k'};

struct stuck {};

// One process: a host with its journal, a loop and a chore on inline schedulers, the journal's IO held so each
// completion is a step of its own, and the shard's group.
struct Node {
	fs::path root;
	task::inline_scheduler loop_s, chore_s;
	j::HeldCompletion held;
	j::InlineCompletion inline_io;
	std::unique_ptr<j::Host> host;
	std::unique_ptr<sg::loop> lp;
	App app;
	// The catalog slot's part (D169): the directory and the index the group borrows, destroyed after it.
	std::unique_ptr<j::IO> io;
	std::optional<prospect::index> ix;
	std::unique_ptr<group_type> g;
	task::cancel_source stop;
	bool turns_done = false;
	bool driver_done = false;
	ProposeEntry propose;

	explicit Node(fs::path r) : root(std::move(r)) {
		fs::create_directories(root / "host");
		fs::create_directories(root / "shard");
		host = std::make_unique<j::Host>(std::make_shared<j::PosixIO>(root / "host", j::PosixIOOptions{j::FlushMode::plain}), j::Identity{'X', 'H'}, j::HostOptions{1, j::TurnLogOptions{8u << 20, 32u << 10, 1, 4, 1u << 20}});
		lp = std::make_unique<sg::loop>(loop_ref(), j::completion_ref::of(inline_io), *host, 0, 7);
		task::spawn(loop_ref(), host->driver(0).run(j::completion_ref::of(held), stop.token()), [this](std::exception_ptr) { turns_done = true; });
		task::spawn(loop_ref(), lp->run(stop.token()), [this](std::exception_ptr) { driver_done = true; });
		app.index = [this]() -> const prospect::index& { return *ix; };
		io = std::make_unique<j::PosixIO>(root / "shard", j::PosixIOOptions{j::FlushMode::plain});
		ix.emplace(group_type::open_index(*io, engine::strata::index_options({}), UUID));
		g = std::make_unique<group_type>(*lp, config(), *io, *ix, app, task::scheduler_ref::of(chore_s));
		propose = [this](std::string entry, sg::ack level) { return g->propose(std::move(entry), level); };
		loop_s.run();
	}

	task::scheduler_ref loop_ref() {
		return task::scheduler_ref::of(loop_s);
	}

	// One step: a chore step, else one held IO completion, else the clock to the next timer. False when idle.
	bool event() {
		loop_s.run();
		std::size_t n = 0;
		if (chore_s.run_until([&] { return n++ == 1; })) {
			return true;
		}
		if (held.held()) {
			held.complete(held.tickets().front());
			return true;
		}
		return false;
	}

	void pump(const std::function<bool()>& done) {
		for (int idle = 0;;) {
			loop_s.run();
			if (done()) {
				return;
			}
			point();
			if (event()) {
				idle = 0;
				continue;
			}
			if (done()) {
				return;
			}
			if (loop_s.armed_timers() == 0 || ++idle > 1000) {
				throw stuck{};
			}
			loop_s.advance(10'000'000);
		}
	}

	template <typename T>
	T run(task::task<T> work) {
		bool done = false;
		std::exception_ptr error;
		std::optional<T> value;
		task::spawn(loop_ref(), std::move(work), [&](std::exception_ptr e, std::optional<T> v) {
			done = true;
			error = e;
			value = std::move(v);
		});
		pump([&] { return done; });
		if (error) {
			std::rethrow_exception(error);
		}
		return std::move(*value);
	}

	void run(task::task<void> work) {
		auto wrap = [](task::task<void> w) -> task::task<bool> {
			co_await std::move(w);
			co_return true;
		};
		run(wrap(std::move(work)));
	}

	void open() {
		run(g->open());
	}

	// Runs every step left.
	void idle() {
		for (;;) {
			loop_s.run();
			point();
			if (!event()) {
				return;
			}
		}
	}

	void shut() {
		stop.request(task::cancel_reason::shutdown);
		for (int round = 0; round < 100000; ++round) {
			loop_s.run();
			chore_s.run();
			if (held.held()) {
				held.complete(held.tickets().front());
				continue;
			}
			if (turns_done && driver_done) {
				break;
			}
		}
		g->fenced("the process stopped");
		for (int round = 0; round < 100000; ++round) {
			if (!loop_s.run() && !chore_s.run() && !held.held()) {
				break;
			}
			if (held.held()) {
				held.fail(held.tickets().front(), std::make_exception_ptr(std::system_error(EIO, std::generic_category(), "stopped")));
			}
		}
		g.reset();
		ix.reset();
		io.reset();
		lp.reset();
		host.reset();
	}
};

// A document whose data names a blob of `ct` by a placeholder locator, as prepare leaves it.
std::string document(const std::string& key, const std::string& ct, std::size_t blob) {
	engine::Document doc;
	doc.set_key(key);
	doc.add_term(doc.field("f"), key);
	Data data;
	data.update(ct_type_t(ct), 0, 0, blob);
	data.flush();
	doc.set_data(data.serialize());
	return doc.serialize();
}

Command put(const std::string& key, const std::string& ct, CommandBytes record, std::size_t size) {
	CommandWrite w;
	w.op = CommandOp::replace_key;
	w.key = key;
	w.document.bytes = document(key, ct, size);
	w.blobs.push_back({ct, std::move(record)});
	Command command;
	command.writes.push_back(std::move(w));
	return command;
}

// A request body in fixed pieces; `at_piece(k)` runs before the k-th.
class Body : public DatabaseHandler::BodyChunks {
	std::string bytes_;
	std::size_t piece_;
	std::size_t at_ = 0;
	std::size_t k_ = 0;

public:
	std::function<void(std::size_t)> at_piece;

	Body(std::string bytes, std::size_t piece) : bytes_(std::move(bytes)), piece_(piece) { }
	task::task<bool> next(std::string& chunk) override {
		if (at_ == bytes_.size()) {
			co_return false;
		}
		if (at_piece) {
			at_piece(k_);
		}
		++k_;
		chunk.assign(bytes_, at_, piece_);
		at_ += chunk.size();
		co_return true;
	}
};

// The script's blobs: a stored-blob write in one entry, chunked writes and uploads past the entry limit.
struct Blob {
	const char* key;
	const char* ct;
	std::size_t size;
	char seed;
};
const Blob BLOBS[] = {
	{"Ka", "a/a", 600 * 1024, 'a'},
	{"Kb", "a/b", 1024, 'b'},
	{"Ku", "video/u", 2560 * 1024 + 11, 'u'},   // a chunked upload, a checkpoint racing it
	{"Kc", "a/c", 1536 * 1024 + 5, 'c'},        // a stored-blob write chunked through the staging
	{"Kd", "a/d", 2048, 'd'},
	{"Kv", "video/v", 1200 * 1024, 'v'},        // an upload left open
	{"Ke", "a/e", 300 * 1024, 'e'},
};

const Blob& blob(const std::string& key) {
	for (const auto& b : BLOBS) {
		if (key == b.key) {
			return b;
		}
	}
	throw std::logic_error("no such blob");
}

// The script, in a child: each acknowledged write's key goes down `fd` as a line, then the steps it took.
void script(const fs::path& root, int fd) {
	auto say = [fd](const std::string& line) {
		const auto text = line + "\n";
		(void)!::write(fd, text.data(), text.size());
	};
	Node n(root);
	n.open();
	auto write = [&](const Blob& b, std::uint64_t id) {
		const auto bytes = pattern(b.size, b.seed);
		const auto a = n.run(propose_command(n.propose, put(b.key, b.ct, CommandBytes{bytes}, b.size), id, sg::ack::quorum));
		if (a.applied && a.applied->result == ::strata::verdict::accepted) {
			say(std::string("ack ") + b.key);
		}
		return a;
	};
	// The first write goes in one entry and names the leader's term for its chunked-command ids.
	const auto first = write(blob("Ka"), 0);
	ChunkedIds ids(first.placement.term, first.placement.index);
	write(blob("Kb"), ids.next());
	{
		const auto& b = blob("Ku");
		ChunkedUpload upload(ids.next(), b.size);
		Body body(pattern(b.size, b.seed), 256 * 1024);
		bool checkpointed = false;
		body.at_piece = [&](std::size_t k) {
			if (k == 5) {
				task::spawn(n.loop_ref(), n.g->checkpoint(), [&](std::exception_ptr, std::optional<cs::CheckpointPublished>) { checkpointed = true; });
			}
		};
		n.run(propose_upload(n.propose, upload, body));
		n.pump([&] { return checkpointed; });
		const auto a = n.run(propose_upload_final(n.propose, upload, put(b.key, b.ct, upload.record(), b.size), sg::ack::quorum));
		if (a.applied && a.applied->result == ::strata::verdict::accepted) {
			say(std::string("ack ") + b.key);
		}
	}
	n.run(n.g->checkpoint());
	write(blob("Kc"), ids.next());
	write(blob("Kd"), ids.next());
	{
		const auto& b = blob("Kv");
		ChunkedUpload upload(ids.next(), b.size);
		Body body(pattern(b.size, b.seed), 512 * 1024);
		n.run(propose_upload(n.propose, upload, body));
	}
	n.run(n.g->checkpoint());
	write(blob("Ke"), ids.next());
	n.idle();
	say("events " + std::to_string(events));
}

struct Trial {
	std::set<std::string> acked;
	std::uint64_t events = 0;
	bool killed = false;
};

// The script in a child process killed at `at` (NEVER: run whole).
Trial run_child(const fs::path& root, std::uint64_t at) {
	fs::remove_all(root);
	int fds[2];
	if (::pipe(fds) != 0) {
		throw std::system_error(errno, std::generic_category(), "pipe");
	}
	std::fflush(nullptr);
	const auto pid = ::fork();
	if (pid < 0) {
		throw std::system_error(errno, std::generic_category(), "fork");
	}
	if (pid == 0) {
		::close(fds[0]);
		events = 0;
		crash_at = at;
		try {
			script(root, fds[1]);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  child: %s\n", e.what());
			::_exit(2);
		} catch (const stuck&) {
			std::fprintf(stderr, "  child: stuck at event %llu\n", static_cast<unsigned long long>(events));
			::_exit(3);
		}
		::_exit(0);
	}
	::close(fds[1]);
	std::string out;
	char buf[4096];
	for (ssize_t got; (got = ::read(fds[0], buf, sizeof(buf))) != 0;) {
		if (got < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		out.append(buf, static_cast<std::size_t>(got));
	}
	::close(fds[0]);
	int status = 0;
	::waitpid(pid, &status, 0);
	Trial trial;
	trial.killed = WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
	if (!trial.killed && !(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		std::fprintf(stderr, "  FAIL: the script's child at %llu ended with status %d\n", static_cast<unsigned long long>(at), status);
		++testh::failures;
	}
	std::istringstream lines(out);
	for (std::string word, value; lines >> word >> value;) {
		if (word == "ack") {
			trial.acked.insert(value);
		} else if (word == "events") {
			trial.events = std::stoull(value);
		}
	}
	return trial;
}

// The bytes of `key`'s blob as its locator names them, if the document is there.
std::optional<std::string> read_blob(group_type& g, const Blob& b) {
	const auto stored = g.strata().get(b.key);
	if (!stored) {
		return std::nullopt;
	}
	Data data{std::string(*stored)};
	const auto* locator = data.get(ct_type_t(b.ct));
	if (locator == nullptr || locator->volume < 1) {
		return std::string("<no locator>");
	}
	try {
		return g.files().read(volume_name(static_cast<std::uint64_t>(locator->volume)), locator->offset, locator->size);
	} catch (const std::exception& e) {
		return std::string("<") + e.what() + ">";
	}
}

int failed_points = 0;

void fail(std::uint64_t at, const std::string& what) {
	++testh::failures;
	if (++failed_points <= 20) {
		std::fprintf(stderr, "  FAIL [crash at %llu]: %s\n", static_cast<unsigned long long>(at), what.c_str());
	}
}

// Reopens the group the child left and checks it.
void verify(const fs::path& root, const Trial& trial, std::uint64_t at) {
	crash_at = NEVER;
	Node n(root);
	std::vector<std::string> damage;
	n.app.recovered = [&](sg::file_set& files) {
		for (const auto& [name, file] : files.capture()) {
			const auto path = root / "shard" / (std::string("app.") + name);
			std::error_code ec;
			const auto size = fs::file_size(path, ec);
			if (ec || size != file.length) {
				damage.push_back(name + " is " + (ec ? std::string("missing") : std::to_string(size)) + " bytes, the bundle names " + std::to_string(file.length));
			}
		}
	};
	try {
		n.open();
		n.idle();
	} catch (const stuck&) {
		fail(at, "the recovery is stuck");
		return;
	} catch (const std::exception& e) {
		fail(at, std::string("the recovery failed: ") + e.what());
		return;
	}
	if (n.g->fenced()) {
		fail(at, "the recovered group is fenced: " + n.g->fence_reason());
	}
	if (n.app.first) {
		fail(at, "the reopened group applied no entry");
	}
	for (const auto& d : damage) {
		fail(at, "after recovery, " + d);
	}
	std::set<std::string> named;   // the files the documents name
	for (const auto& b : BLOBS) {
		const auto got = read_blob(*n.g, b);
		if (!got) {
			if (trial.acked.count(b.key)) {
				fail(at, std::string("acknowledged ") + b.key + " is gone");
			}
			continue;
		}
		if (*got != pattern(b.size, b.seed)) {
			fail(at, std::string(b.key) + "'s blob reads back other bytes: " + got->substr(0, 40));
		}
		Data data{std::string(*n.g->strata().get(b.key))};
		named.insert(volume_name(static_cast<std::uint64_t>(data.get(ct_type_t(b.ct))->volume)));
	}
	if (!n.g->state().open.empty()) {
		fail(at, "an open chunked command outlived the reopened term's first entry");
	}
	named.insert(volume_name(n.g->state().volume));
	for (const auto& [name, file] : n.g->files().capture()) {
		if (!named.count(name)) {
			fail(at, "live file " + name + " is named by no document");
		}
	}
	n.shut();
}

// A command past the limit goes as chunks through consensus; proposed whole, consensus refuses it.
void test_chunks_through_the_group() {
	const auto root = fs::current_path() / "group_chunk_group_limit";
	fs::remove_all(root);
	{
		Node n(root);
		n.open();
		const auto& b = blob("Kc");
		const auto bytes = pattern(b.size, b.seed);
		const auto command = put(b.key, b.ct, CommandBytes{bytes}, b.size);
		const auto entries = split_command(command, chunked_id(1, 1));
		CHECK_GT(entries.size(), 2u);

		bool refused = false;
		try {
			n.run(n.g->propose(encode_entry(command, ~std::size_t(0)), sg::ack::quorum));
		} catch (const std::exception&) {
			refused = true;
		}
		CHECK(refused);
		const auto before = n.g->applied();

		const auto probe = n.run(n.g->propose(encode_entry(Command{}), sg::ack::quorum));
		ChunkedIds ids(probe.placement.term, probe.placement.index);
		const auto a = n.run(propose_command(n.propose, command, ids.next(), sg::ack::quorum));
		REQUIRE(a.applied.has_value());
		CHECK(a.applied->result == ::strata::verdict::accepted);
		CHECK_GE(n.g->applied(), before + entries.size());
		CHECK(read_blob(*n.g, b) == bytes);

		const auto& u = blob("Ku");
		ChunkedUpload upload(ids.next(), u.size);
		Body body(pattern(u.size, u.seed), 100 * 1024 + 3);
		n.run(propose_upload(n.propose, upload, body));
		const auto f = n.run(propose_upload_final(n.propose, upload, put(u.key, u.ct, upload.record(), u.size), sg::ack::quorum));
		REQUIRE(f.applied.has_value());
		CHECK(f.applied->result == ::strata::verdict::accepted);
		CHECK(read_blob(*n.g, u) == pattern(u.size, u.seed));
		CHECK(n.g->state().open.empty());
		n.shut();
	}
	fs::remove_all(root);
}

// kill -9 at each crash point of the script.
void test_kill_at_each_crash_point() {
	const auto root = fs::current_path() / "group_chunk_group_crash";
	const auto whole = run_child(root, NEVER);
	CHECK_EQ(whole.acked.size(), 6u);   // every write but the upload left open
	CHECK_GT(whole.events, 0u);
	verify(root, whole, NEVER);

	const auto env = [](const char* name, std::uint64_t otherwise) {
		const char* value = std::getenv(name);
		return value ? std::stoull(value) : otherwise;
	};
	const auto first = env("GROUP_CHUNK_FIRST", 1);
	const auto stride = env("GROUP_CHUNK_STRIDE", 1);
	std::uint64_t points = 0;
	for (auto at = first; at <= whole.events; at += stride) {
		const auto trial = run_child(root, at);
		if (!trial.killed) {
			fail(at, "the child was not killed at its crash point");
			continue;
		}
		verify(root, trial, at);
		++points;
	}
	std::printf("group chunk crash: %llu script events, %llu crash points killed and verified\n", static_cast<unsigned long long>(whole.events),
		static_cast<unsigned long long>(points));
	fs::remove_all(root);
}

}  // namespace

TEST_MAIN(test_chunks_through_the_group, test_kill_at_each_crash_point)
