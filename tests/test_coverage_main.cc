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


// Coverage tests for src/main.cc: the pieces of process start-up that a test can call directly (the signal texts, the
// info-hook toggle, the signal handler without a manager, the open-files limit, the data directory and the pid file).
// main.cc is compiled into this binary under a renamed main() and a renamed `opts`, so its globals do not collide with
// the test stubs'. The whole process (flags, signals, shutdown, dump and restore) is covered by
// harness/coverage_e2e_main.sh.

#include "test_harness.h"

// main.cc's own includes first, so that the renames below touch only main.cc's text.
#include "config.h"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <memory>
#include <pwd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#include <sys/prctl.h>
#endif
#endif

#ifdef XAPIAND_LUA
#include <lua.h>
#endif

#include "database/handler.h"
#include "engine/sync.h"
#include "database/schema.h"
#include "endpoint.h"
#include "error.hh"
#include "xapian_data_guard.h"
#include "xapiand_exception.h"
#include "fs.hh"
#include "hashes.hh"
#include "io.hh"
#include "log.h"
#include "logger.h"
#include "manager.h"
#include "nanosleep.h"
#include "opts.h"
#include "package.h"
#include "xapiand_strings.hh"
#include "system.hh"
#include "traceback.h"
#include "tls.h"

// main.cc's exit(status) ends the process at once with that status, so a test sees the status the code chose and not
// what the exit machinery (atexit handlers, sanitizer runtimes, static destructors) does with no descriptors left.
namespace std {
[[noreturn]] inline void main_exit(int status) {  // also reached as std::main_exit, for main.cc's std::exit
	std::fflush(nullptr);
	::_exit(status);
}
}  // namespace std
using std::main_exit;

#define main xapiand_main_not_run
#define opts main_opts
#define exit main_exit
#include "../src/main.cc"
#undef exit
#undef opts
#undef main

#include <string>
#include <string_view>

namespace {

// Runs `fn` in a forked child (it may lower the open-files limit or exit) and returns the child's exit status:
// 0 when every CHECK in it held, 1 when one failed, otherwise the status `fn` exited with.
template <typename F>
int in_child(F fn) {
	std::fflush(nullptr);
	pid_t pid = ::fork();
	if (pid == 0) {
		testh::failures = 0;
		fn();
		std::fflush(nullptr);
		std::exit(testh::failures != 0 ? 1 : 0);
	}
	int status = 0;
	::waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		std::fprintf(stderr, "DIAG in_child: %s %d\n", WIFEXITED(status) ? "exited" : "killed by signal",
			WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status));
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

// What `fn` writes to stderr.
template <typename F>
std::string capture_stderr(F fn) {
	int fds[2];
	if (::pipe(fds) != 0) {
		return "pipe failed";
	}
	int saved = ::dup(STDERR_FILENO);
	::dup2(fds[1], STDERR_FILENO);
	::close(fds[1]);
	fn();
	::dup2(saved, STDERR_FILENO);
	::close(saved);
	std::string out;
	char buf[4096];
	::fcntl(fds[0], F_SETFL, O_NONBLOCK);
	ssize_t n;
	while ((n = ::read(fds[0], buf, sizeof buf)) > 0) {
		out.append(buf, static_cast<std::size_t>(n));
	}
	::close(fds[0]);
	return out;
}

std::string signal_name(int signum) {
#if defined(__APPLE__) || defined(__FreeBSD__)
	return sys_signame[signum];
#else
	return strsignal(signum);
#endif
}

std::string write_to_pipe(int signum) {
	int fds[2];
	if (::pipe(fds) != 0) {
		return "pipe failed";
	}
	signals.write(fds[1], signum);
	::close(fds[1]);
	std::string out;
	char buf[1024];
	ssize_t n;
	while ((n = ::read(fds[0], buf, sizeof buf)) > 0) {
		out.append(buf, static_cast<std::size_t>(n));
	}
	::close(fds[0]);
	return out;
}

bool contains(const std::string& haystack, const std::string& needle) {
	return haystack.find(needle) != std::string::npos;
}

void test_signal_texts() {
	for (int signum : {SIGTERM, SIGINT, SIGUSR1, SIGSTOP, SIGCHLD, SIGWINCH, SIGSEGV, SIGABRT}) {
		const auto name = signal_name(signum);
		CHECK_EQ(signals.name(signum), name);
		const auto text = write_to_pipe(signum);
		// On a terminal the text is wrapped in color codes; the words are the same.
		CHECK(contains(text, "Signal received: " + name + (is_tty ? "" : "\n")));
		if (!is_tty) {
			CHECK_EQ(text, "Signal received: " + name + "\n");
		}
	}
	// A number outside 0..NSIG-1 reports the last entry.
	CHECK_EQ(signals.name(-1), signals.name(NSIG - 1));
	CHECK_EQ(signals.name(NSIG + 7), signals.name(NSIG - 1));
	CHECK_EQ(write_to_pipe(-1), write_to_pipe(NSIG - 1));
	CHECK_EQ(write_to_pipe(NSIG + 7), write_to_pipe(NSIG - 1));
}

void test_toggle_hooks() {
	CHECK_EQ(logger_info_hook.load(), static_cast<uint64_t>(0));
	auto enabled = capture_stderr([] { toggle_hooks(0); });
	CHECK_EQ(logger_info_hook.load(), static_cast<uint64_t>(fnv1ah32::hash("")));
	CHECK(contains(enabled, "Info hooks enabled!"));
	auto disabled = capture_stderr([] { toggle_hooks(0); });
	CHECK_EQ(logger_info_hook.load(), static_cast<uint64_t>(0));
	CHECK(contains(disabled, "Info hooks disabled!"));
}

void lower_limit(rlim_t to);

// Descriptors 0, 1 and 2 all open (a runner may start the test with one of them closed), so that a limit of 3 leaves
// no descriptor to open.
void fill_stdio() {
	for (int fd = 0; fd < 3; ++fd) {
		if (::fcntl(fd, F_GETFD) == -1) {
			(void)::open("/dev/null", O_RDWR);
		}
	}
}

void test_signal_handler_without_a_manager() {
	REQUIRE(!XapiandManager::manager());
	for (int signum : {SIGUSR1, SIGWINCH, SIGCHLD}) {
		errno = EBADF;
		auto text = capture_stderr([signum] { sig_handler(signum); });
		CHECK(contains(text, "Signal received: " + signal_name(signum)));
		CHECK_EQ(errno, EBADF);  // the handler restores errno
	}
	// With no descriptor left to open /dev/null, SIGTERM ends the process with EX_OSFILE.
	CHECK_EQ(in_child([] {
		fill_stdio();
		lower_limit(3);
		struct rlimit rl;
		::getrlimit(RLIMIT_NOFILE, &rl);
		errno = 0;
		std::atexit([] { std::fprintf(stderr, "DIAG sig_handler child: atexit reached\n"); });
		const int probe = ::open("/dev/null", O_RDONLY);  // what sig_handler's own open sees
		std::fprintf(stderr, "DIAG sig_handler child: rlim_cur %llu rlim_max %llu, probe open fd %d errno %d, fcntl 0/1/2: %d %d %d\n",
			static_cast<unsigned long long>(rl.rlim_cur), static_cast<unsigned long long>(rl.rlim_max), probe, errno,
			::fcntl(0, F_GETFD), ::fcntl(1, F_GETFD), ::fcntl(2, F_GETFD));
		if (probe != -1) {
			::close(probe);
		}
		sig_handler(SIGTERM);
	}), static_cast<int>(EX_OSFILE));
	// With no manager to hand it to, SIGTERM and SIGINT end the process with EX_SOFTWARE.
	CHECK_EQ(in_child([] { capture_stderr([] { sig_handler(SIGTERM); }); }), static_cast<int>(EX_SOFTWARE));
	CHECK_EQ(in_child([] { capture_stderr([] { sig_handler(SIGINT); }); }), static_cast<int>(EX_SOFTWARE));
}

std::string scratch_dir() {
	std::string tmpl = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : ".") + "/coverage_main.XXXXXX";
	CHECK(::mkdtemp(tmpl.data()) != nullptr);
	char buf[PATH_MAX + 1];
	return ::realpath(tmpl.c_str(), buf) ? std::string(buf) : tmpl;
}

void make_path(const std::string& path) {
	for (std::size_t pos = 1; (pos = path.find('/', pos)) != std::string::npos; ++pos) {
		::mkdir(path.substr(0, pos).c_str(), 0755);
	}
	::mkdir(path.c_str(), 0755);
}

void touch_file(const std::string& path, std::string_view content = "") {
	make_path(path.substr(0, path.rfind('/')));
	if (auto* f = std::fopen(path.c_str(), "w")) {
		std::fwrite(content.data(), 1, content.size(), f);
		std::fclose(f);
	}
}

void remove_tree(const std::string& dir) {
	if (dir.find("coverage_main.") != std::string::npos) {
		std::string cmd = "rm -rf '" + dir + "'";
		(void)std::system(cmd.c_str());
	}
}

// The exit code usedir() throws with, or -1 when it returns.
int usedir_code(const std::string& path, bool force) {
	try {
		usedir(path, force);
	} catch (const SystemExit& exc) {
		return exc.code;
	}
	return -1;
}

void test_usedir() {
	char start[PATH_MAX + 1];
	REQUIRE(::getcwd(start, sizeof start) != nullptr);
	const auto root = scratch_dir();

	// An empty path is a usage error.
	CHECK_EQ(usedir_code("", false), static_cast<int>(EX_USAGE));

	// A new directory is created with its .xapiand, becomes the working directory and Endpoint::cwd ends with a slash.
	CHECK_EQ(usedir_code(root + "/fresh", false), -1);
	char here[PATH_MAX + 1];
	REQUIRE(::getcwd(here, sizeof here) != nullptr);
	CHECK_EQ(std::string(here), root + "/fresh");
	CHECK_EQ(Endpoint::cwd, root + "/fresh/");
	struct stat st;
	CHECK_EQ(::stat((root + "/fresh/.xapiand").c_str(), &st), 0);
	CHECK(S_ISDIR(st.st_mode));
	CHECK_EQ(::chdir(start), 0);

	// A path that names the .xapiand directory is the data directory itself.
	CHECK_EQ(usedir_code(root + "/fresh/.xapiand", false), -1);
	CHECK_EQ(Endpoint::cwd, root + "/fresh/");
	CHECK_EQ(::chdir(start), 0);

	// A node file marks a directory a node already uses; any other file does not.
	touch_file(root + "/used/.xapiand/node", "name");
	CHECK_EQ(usedir_code(root + "/used", false), -1);
	CHECK_EQ(::chdir(start), 0);
	touch_file(root + "/stray/.xapiand/junk", "x");
	CHECK_EQ(usedir_code(root + "/stray", false), static_cast<int>(EX_DATAERR));
	CHECK_EQ(::chdir(start), 0);
	// --force takes it anyway.
	CHECK_EQ(usedir_code(root + "/stray", true), -1);
	CHECK_EQ(Endpoint::cwd, root + "/stray/");
	CHECK_EQ(::chdir(start), 0);
	CHECK_EQ(usedir_code(root + "/forced/deeper/still", true), -1);
	CHECK_EQ(Endpoint::cwd, root + "/forced/deeper/still/");
	CHECK_EQ(::chdir(start), 0);

	// The files of a Xapian-based release stop the start, with or without --force.
	touch_file(root + "/xapian/.xapiand/indexes/iamglass");
	CHECK_EQ(usedir_code(root + "/xapian", false), static_cast<int>(EX_DATAERR));
	CHECK_EQ(usedir_code(root + "/xapian", true), static_cast<int>(EX_DATAERR));

	// A directory that cannot be created (its parent is a file).
	touch_file(root + "/plain", "x");
	CHECK_EQ(usedir_code(root + "/plain/below", false), static_cast<int>(EX_OSFILE));
	CHECK_EQ(usedir_code(root + "/plain/below", true), static_cast<int>(EX_OSFILE));

	CHECK_EQ(::chdir(start), 0);
	remove_tree(root);
}

void test_cli_endpoint() {
	char start[PATH_MAX + 1];
	REQUIRE(::getcwd(start, sizeof start) != nullptr);
	const auto root = scratch_dir();
	REQUIRE_EQ(usedir_code(root + "/data", false), -1);

	// An index inside the data directory resolves; one outside it, or none, is a usage error.
	CHECK_EQ(cli_endpoint("idx").path, std::string("idx"));
	for (std::string_view name : {"", "..", "../x", "/etc/passwd"}) {
		int code = -1;
		try {
			cli_endpoint(name);
		} catch (const SystemExit& exc) {
			code = exc.code;
		}
		CHECK_EQ(code, static_cast<int>(EX_USAGE));
	}

	CHECK_EQ(::chdir(start), 0);
	remove_tree(root);
}

void test_writepid() {
	const auto root = scratch_dir();
	writepid((root + "/x.pid").c_str());
	std::string content;
	if (auto* f = std::fopen((root + "/x.pid").c_str(), "r")) {
		char buf[64];
		auto n = std::fread(buf, 1, sizeof buf, f);
		content.assign(buf, n);
		std::fclose(f);
	}
	CHECK_EQ(content, std::to_string(static_cast<unsigned long>(::getpid())) + "\n");
	// A pid file that cannot be created is skipped without a word.
	writepid((root + "/missing/dir/x.pid").c_str());
	remove_tree(root);
}

void test_data_path_beside_the_executable() {
	// Apple and Linux know their executable's path; elsewhere (FreeBSD has no /proc/self/exe) it comes from argv[0]
	// resolved with realpath, so the test hands it the path the kernel reports.
	auto exe = get_executable_path(nullptr);
#if defined(__FreeBSD__)
	if (exe.empty()) {
		int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
		char path[PATH_MAX + 1];
		size_t len = sizeof path - 1;
		if (::sysctl(mib, 4, path, &len, nullptr, 0) == 0) {
			path[len] = '\0';
			exe = get_executable_path(path);
		}
	}
#endif
	CHECK(exe.size() > 1 && exe.front() == '/');
	CHECK(exe.size() > 26 && exe.substr(exe.size() - 26) == "xapiand_test_coverage_main");

	// An explicit XAPIAND_DATA_PATH always wins.
	::setenv("XAPIAND_DATA_PATH", "/explicit/path", 1);
	setup_data_path("ignored");
	CHECK_EQ(std::string(::getenv("XAPIAND_DATA_PATH")), std::string("/explicit/path"));

	// Without it, the first root beside the executable that holds mime.types or stopwords is taken.
	::unsetenv("XAPIAND_DATA_PATH");
	setup_data_path("ignored");
	if (auto* found = ::getenv("XAPIAND_DATA_PATH")) {
		const std::string root(found);
		CHECK(::access((root + "/mime.types").c_str(), R_OK) == 0 || ::access((root + "/stopwords").c_str(), R_OK) == 0);
		::unsetenv("XAPIAND_DATA_PATH");
	}
}

void test_demote() {
	if (::getuid() != 0 && ::geteuid() != 0) {
		// Not root: there is no privilege to lose and nothing changes.
		const auto uid = ::getuid();
		demote("nobody", "nogroup");
		demote(nullptr, nullptr);
		CHECK_EQ(::getuid(), uid);
		return;
	}
	int code = -1;
	try { demote(nullptr, nullptr); } catch (const SystemExit& exc) { code = exc.code; }
	CHECK_EQ(code, static_cast<int>(EX_USAGE));
	CHECK_EQ(in_child([] {
		try { demote("no-such-user-coverage", ""); } catch (const SystemExit& exc) { std::exit(exc.code); }
	}), static_cast<int>(EX_NOUSER));
	CHECK_EQ(in_child([] {
		try { demote("root", "no-such-group-coverage"); } catch (const SystemExit& exc) { std::exit(exc.code); }
	}), static_cast<int>(EX_NOUSER));
}

// The open-files limit, one case per child: the child lowers the process's own limit.
void lower_limit(rlim_t to) {
	struct rlimit rl;
	::getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = std::min<rlim_t>(to, rl.rlim_max);
	rl.rlim_max = rl.rlim_cur;
	::setrlimit(RLIMIT_NOFILE, &rl);
}

// adjustOpenFilesLimit refusing the start: the child ends with 0 when it threw SystemExit(EX_OSFILE), with the descriptors
// the process holds (3 here: stdin, stdout and stderr) and the limit it ended with checked.
void expect_refusal(ssize_t limit, ssize_t slack = 0) {
	const auto diag = [limit](const char* what, int code) {
		struct rlimit rl;
		::getrlimit(RLIMIT_NOFILE, &rl);
		std::fprintf(stderr, "DIAG expect_refusal(%zd) %s: code %d, rlim_cur %llu rlim_max %llu, open %zd, max_per_proc %zd, "
			"pool %zd clients %zd max_files %zd\n", limit, what, code, static_cast<unsigned long long>(rl.rlim_cur),
			static_cast<unsigned long long>(rl.rlim_max), static_cast<ssize_t>(get_open_files_per_proc()),
			static_cast<ssize_t>(get_max_files_per_proc()), main_opts.database_pool_size, main_opts.max_clients, main_opts.max_files);
	};
	try {
		adjustOpenFilesLimit();
	} catch (const SystemExit& exc) {
		diag("threw", exc.code);
		CHECK_EQ(exc.code, static_cast<int>(EX_OSFILE));
		CHECK_LE(static_cast<ssize_t>(get_open_files_per_proc()), static_cast<ssize_t>(64));
		// get_max_files_per_proc is min(sysconf(_SC_OPEN_MAX), RLIMIT_NOFILE soft). An explicit limit is set exactly; an
		// automatic one is the first value of a 16-step walk down from the recommended number that setrlimit accepts.
		const auto max_files = static_cast<ssize_t>(get_max_files_per_proc());
		CHECK_LE(max_files, limit);
		CHECK_GT(max_files, limit - slack - 1);
		return;
	}
	diag("did not throw", 0);
	CHECK(false);
}

// Whether the process may raise its (just lowered) hard limit to `to`, as root can; the limit is put back after.
bool can_raise_to(rlim_t to) {
	struct rlimit before;
	::getrlimit(RLIMIT_NOFILE, &before);
	struct rlimit rl = {to, to};
	const bool raised = ::setrlimit(RLIMIT_NOFILE, &rl) == 0;
	::setrlimit(RLIMIT_NOFILE, &before);
	return raised;
}

// adjustOpenFilesLimit for a case that must not refuse: a refusal ends the child with status 2.
void adjust_or_report() {
	try {
		adjustOpenFilesLimit();
	} catch (const SystemExit& exc) {
		std::fprintf(stderr, "adjustOpenFilesLimit threw SystemExit(%d)\n", exc.code);
		std::exit(2);
	}
}

void test_open_files_limit() {
	// An explicit limit too low for even one database and one client ends the start with EX_OSFILE.
	CHECK_EQ(in_child([] {
		main_opts.max_files = 60;
		main_opts.database_pool_size = 200;
		main_opts.max_clients = 1000;
		try { adjustOpenFilesLimit(); } catch (const SystemExit& exc) { std::exit(exc.code); }
		std::exit(1);
	}), static_cast<int>(EX_OSFILE));

	// An explicit limit that the configured pool and clients do not fit in starts with what it holds: (300 - 68) / 7 = 33
	// databases leave room for (300 - 297) / 2 = 1 client.
	CHECK_EQ(in_child([] {
		main_opts.max_files = 300;
		main_opts.database_pool_size = 200;
		main_opts.max_clients = 1000;
		main_opts.concurrent_search_indexes = 100;
		adjust_or_report();
		CHECK_EQ(main_opts.max_files, static_cast<ssize_t>(300));
		CHECK_EQ(main_opts.database_pool_size, static_cast<ssize_t>(33));
		CHECK_EQ(main_opts.max_clients, static_cast<ssize_t>(1));
		CHECK_EQ(main_opts.concurrent_search_indexes, static_cast<ssize_t>(33));
	}), 0);

	// The minimum is what holds one database and one client on top of the descriptors the process has open: one
	// descriptor less is refused, and the log names that minimum.
	CHECK_EQ(in_child([] {
		const ssize_t minimum = static_cast<ssize_t>(get_open_files_per_proc()) + 75;
		main_opts.max_files = minimum;
		main_opts.database_pool_size = 200;
		main_opts.max_clients = 1000;
		adjust_or_report();
		// 3 open descriptors give 78: one database, which leaves (78 - 73) / 2 = 2 clients.
		const ssize_t pool = (minimum - 68) / 7;
		CHECK_EQ(main_opts.database_pool_size, pool);
		CHECK_EQ(main_opts.max_clients, (minimum - (57 + (pool + 1) * 7 + 2)) / 2);
		main_opts.max_files = minimum - 1;
		main_opts.database_pool_size = 200;
		main_opts.max_clients = 1000;
		expect_refusal(minimum - 1);
	}), 0);

	// A roomy explicit limit changes nothing the pool and the clients asked for.
	CHECK_EQ(in_child([] {
		main_opts.max_files = 4096;
		main_opts.database_pool_size = 20;
		main_opts.max_clients = 30;
		main_opts.concurrent_search_indexes = 50;
		adjustOpenFilesLimit();
		CHECK_EQ(main_opts.database_pool_size, static_cast<ssize_t>(20));
		CHECK_EQ(main_opts.max_clients, static_cast<ssize_t>(30));
		CHECK_EQ(main_opts.concurrent_search_indexes, static_cast<ssize_t>(20));
		CHECK_EQ(main_opts.max_files, static_cast<ssize_t>(4096));
	}), 0);

	// With no explicit limit and a hard limit of 1024 the default pool and clients are cut to what the limit holds,
	// unless the process may raise its hard limit (root): then the raise to the system's per-process maximum succeeds.
	CHECK_EQ(in_child([] {
		lower_limit(1024);
		const bool can_raise = can_raise_to(1500);  // above the 1471 the whole pool and clients ask for
		main_opts.max_files = 0;
		main_opts.database_pool_size = 200;
		main_opts.max_clients = 1000;
		adjust_or_report();
		if (can_raise) {
			CHECK_GE(static_cast<ssize_t>(get_max_files_per_proc()), static_cast<ssize_t>(1471));
		} else {
			CHECK_LE(main_opts.max_files, static_cast<ssize_t>(1024));
			CHECK_GE(main_opts.database_pool_size, static_cast<ssize_t>(1));
			CHECK_LT(main_opts.database_pool_size, static_cast<ssize_t>(200));
			CHECK_GE(main_opts.max_clients, static_cast<ssize_t>(1));
		}
	}), 0);

	// Explicit limits the process can only lower, and a request above them: the raise fails and the limit it gets is
	// reported, and the pool and the clients still fit.
	CHECK_EQ(in_child([] {
		lower_limit(512);
		const bool can_raise = can_raise_to(5000);
		main_opts.max_files = 5000;
		main_opts.database_pool_size = 2;
		main_opts.max_clients = 2;
		adjust_or_report();
		CHECK_LE(main_opts.max_files, static_cast<ssize_t>(can_raise ? 5000 : 512));
		CHECK_EQ(main_opts.database_pool_size, static_cast<ssize_t>(2));
		CHECK_EQ(main_opts.max_clients, static_cast<ssize_t>(2));
	}), 0);

	// An explicit limit below the current one is applied.
	CHECK_EQ(in_child([] {
		main_opts.max_files = 400;
		main_opts.database_pool_size = 2;
		main_opts.max_clients = 2;
		adjust_or_report();
		CHECK_EQ(main_opts.max_files, static_cast<ssize_t>(400));
		struct rlimit rl;
		::getrlimit(RLIMIT_NOFILE, &rl);
		CHECK_EQ(rl.rlim_cur, static_cast<rlim_t>(400));
	}), 0);

	// A hard limit between the minimum and the recommended number: the start goes on with a smaller pool and clients.
	for (rlim_t limit : {2000, 3000, 4000}) {
		CHECK_EQ(in_child([limit] {
			lower_limit(limit);
			const bool can_raise = can_raise_to(5000);
			main_opts.max_files = 0;
			main_opts.database_pool_size = 200;
			main_opts.max_clients = 1000;
			adjust_or_report();
			if (!can_raise) {
				CHECK_LE(main_opts.max_files, static_cast<ssize_t>(limit));
			}
			CHECK_LE(main_opts.database_pool_size, static_cast<ssize_t>(200));
			CHECK_LE(main_opts.max_clients, static_cast<ssize_t>(1000));
		}), 0);
	}
}

}  // namespace

TEST_MAIN(test_signal_texts, test_toggle_hooks, test_signal_handler_without_a_manager, test_usedir, test_cli_endpoint,
	test_writepid, test_data_path_beside_the_executable, test_demote, test_open_files_limit)
