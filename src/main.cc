/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"                               // for XAPIAND_CHAISCRIPT, XAPIAND_UUID...

#include <chrono>                                 // for std::chrono::
#include <clocale>                                // for std::setlocale, LC_CTYPE
#include <csignal>                                // for sigaction, sigemptyset
#include <cstdlib>                                // for std::size_t, std::atoi, std::getenv, std::exit, std::setenv
#include <cstring>                                // for std::strcmp
#include <errno.h>                                // for errno
#include <fcntl.h>                                // for O_RDWR, O_CREAT
#include <grp.h>                                  // for getgrgid, group, getgrnam, gid_t
#include <memory>                                 // for std::make_unique
#include <pwd.h>                                  // for passwd, getpwnam, getpwuid
#include <signal.h>                               // for NSIG, sigaction, signal, SIG_IGN, SIGHUP
#include <sys/resource.h>                         // for rlimit
#include <sysexits.h>                             // for EX_NOUSER, EX_OK, EX_USAGE, EX_OSFILE
#include <unistd.h>                               // for unlink, STDERR_FILENO, chdir
#include <vector>                                 // for vector

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#include <sys/prctl.h>
#endif

#ifdef XAPIAND_CHAISCRIPT
#include "chaiscript/chaiscript_defines.hpp"      // for chaiscript::Build_Info
#endif

#include "check_size.h"                           // for check_size
#include "database/handler.h"                     // for DatabaseHandler
#include "endpoint.h"                             // for Endpoint, Endpoint::cwd
#include "error.hh"                               // for error::name, error::description
#include "ev/ev++.h"                              // for ::DEVPOLL, ::EPOLL, ::KQUEUE
#include "exception.h"                            // for SystemExit
#include "fs.hh"                                  // for mkdirs
#include "hashes.hh"                              // for fnv1ah32
#include "io.hh"                                  // for io::dup2, io::open, io::close, io::write
#include "log.h"                                  // for L_INFO, L_CRIT, L_NOTICE
#include "logger.h"                               // for Logging
#include "manager.h"                              // for XapiandManager
#include "opts.h"                                 // for opts_t
#include "package.h"                              // for Package::
#include "schema.h"                               // for default_spc
#include "string.hh"                              // for string::format, string::center
#include "system.hh"                              // for get_max_files_per_proc, get_open_files_system_wide
#include "xapian.h"                               // for XAPIAN_HAS_GLASS_BACKEND, XAPIAN...

#if defined(__linux__) && !defined(__GLIBC__)
#include <pthread.h>                              // for pthread_attr_t, pthread_setattr_default_np
#endif


#define FDS_RESERVED     50             // Is there a better approach?
#define FDS_PER_CLIENT    2             // KQUEUE + IPv4
#define FDS_PER_DATABASE  7             // Writable~=7, Readable~=5


opts_t opts;

static const bool is_tty = isatty(STDERR_FILENO) != 0;


template<typename T, std::size_t N>
static ssize_t write(int fildes, T (&buf)[N]) {
	return write(fildes, buf, N - 1);
}

static ssize_t write(int fildes, std::string_view str) {
	return write(fildes, str.data(), str.size());
}


template <std::size_t N>
class signals_t {
	static constexpr int signals = N;

	std::array<std::string, signals> tty_messages;
	std::array<std::string, signals> messages;

public:
	signals_t() {
		for (std::size_t sig = 0; sig < signals; ++sig) {
#if defined(__linux__)
				const char* sig_str = strsignal(sig);
#elif defined(__APPLE__) || defined(__FreeBSD__)
				const char* sig_str = sys_signame[sig];
#endif
			switch (sig) {
				case SIGQUIT:
				case SIGILL:
				case SIGTRAP:
				case SIGABRT:
#if defined(__APPLE__) || defined(__FreeBSD__)
				case SIGEMT:
#endif
				case SIGFPE:
				case SIGBUS:
				case SIGSEGV:
				case SIGSYS:
					// create core image
					tty_messages[sig] = string::format(LIGHT_RED + "Signal received: {}" + CLEAR_COLOR + "\n", sig_str);
					messages[sig] = string::format("Signal received: {}\n", sig_str);
					break;
				case SIGHUP:
				case SIGINT:
				case SIGKILL:
				case SIGPIPE:
				case SIGALRM:
				case SIGTERM:
				case SIGXCPU:
				case SIGXFSZ:
				case SIGVTALRM:
				case SIGPROF:
				case SIGUSR1:
				case SIGUSR2:
#if defined(__linux__)
				case SIGSTKFLT:
#endif
					// terminate process
					tty_messages[sig] = string::format(BROWN + "Signal received: {}" + CLEAR_COLOR + "\n", sig_str);
					messages[sig] = string::format("Signal received: {}\n", sig_str);
					break;
				case SIGSTOP:
				case SIGTSTP:
				case SIGTTIN:
				case SIGTTOU:
					// stop process
					tty_messages[sig] = string::format(SADDLE_BROWN + "Signal received: {}" + CLEAR_COLOR + "\n", sig_str);
					messages[sig] = string::format("Signal received: {}\n", sig_str);
					break;
				case SIGURG:
				case SIGCONT:
				case SIGCHLD:
				case SIGIO:
				case SIGWINCH:
#if defined(__APPLE__) || defined(__FreeBSD__)
				case SIGINFO:
#endif
					// discard signal

					tty_messages[sig] = string::format(STEEL_BLUE + "Signal received: {}" + CLEAR_COLOR + "\n", sig_str);
					messages[sig] = string::format("Signal received: {}\n", sig_str);
					break;
				default:
					tty_messages[sig] = string::format(STEEL_BLUE + "Signal received: {}" + CLEAR_COLOR + "\n", sig_str);
					messages[sig] = string::format("Signal received: {}\n", sig_str);
					break;
			}
		}
	}

	void write(int fildes, int sig) {
		if (is_tty) {
			::write(fildes, tty_messages[(sig >= 0 && sig < signals) ? sig : signals - 1]);
		} else {
			::write(fildes, messages[(sig >= 0 && sig < signals) ? sig : signals - 1]);
		}
	}
};

static signals_t<NSIG> signals;


void toggle_hooks(int /*unused*/) {
	if (logger_info_hook != 0u) {
		logger_info_hook = 0;
		if (is_tty) {
			::write(STDERR_FILENO, STEEL_BLUE + "Info hooks disabled!" + CLEAR_COLOR + "\n");
		} else {
			::write(STDERR_FILENO, "Info hooks disabled!\n");
		}
	} else {
		logger_info_hook = fnv1ah32::hash("");
		if (is_tty) {
			::write(STDERR_FILENO, STEEL_BLUE + "Info hooks enabled!" + CLEAR_COLOR + "\n");
		} else {
			::write(STDERR_FILENO, "Info hooks enabled!\n");
		}
	}
}


void sig_handler(int sig) {
	int old_errno = errno; // save errno because write will clobber it

	signals.write(STDERR_FILENO, sig);

	if (sig == SIGTERM || sig == SIGINT) {
		::close(STDIN_FILENO);
	}

// #if defined(__APPLE__) || defined(__FreeBSD__)
//  if (sig == SIGINFO) {
//      toggle_hooks(sig);
//  }
// #endif

	auto manager = XapiandManager::manager();
	if (manager && !manager->is_deinited()) {
		try {
			manager->signal_sig(sig);
		} catch (const SystemExit& exc) {
			// Flag atom_sig for clean exit in the next Manager::join() timeout
			manager->atom_sig = -exc.code;
		}
	} else {
		if (sig == SIGTERM || sig == SIGINT) {
			exit(EX_SOFTWARE);
		}
	}

	errno = old_errno;
}


void setup_signal_handlers() {
	signal(SIGHUP, SIG_IGN);   // Ignore terminal line hangup
	signal(SIGPIPE, SIG_IGN);  // Ignore write on a pipe with no reader

	struct sigaction sa;

	/* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
	 * Otherwise, sa_handler is used. */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;          // If restarting works we save iterations
	sa.sa_handler = sig_handler;
	sigaction(SIGTERM, &sa, nullptr);  // On software termination signal
	sigaction(SIGINT, &sa, nullptr);   // On interrupt program (Ctrl-C)
#if defined(__APPLE__) || defined(__FreeBSD__)
	sigaction(SIGINFO, &sa, nullptr);  // On status request from keyboard (Ctrl-T)
#endif
	sigaction(SIGUSR1, &sa, nullptr);
	sigaction(SIGUSR2, &sa, nullptr);
}


/*
 * From https://github.com/antirez/redis/blob/b46239e58b00774d121de89e0e033b2ed3181eb0/src/server.c#L1496
 *
 * This function will try to raise the max number of open files accordingly to
 * the configured max number of clients. It also reserves a number of file
 * descriptors for extra operations of persistence, listening sockets, log files and so forth.
 *
 * If it will not be possible to set the limit accordingly to the configured
 * max number of clients, the function will do the reverse setting
 * to the value that we can actually handle.
 */
void adjustOpenFilesLimit() {

	// Try getting the currently available number of files (-10%):
	ssize_t available_files = get_max_files_per_proc() - get_open_files_system_wide();
	ssize_t aprox_available_files = (available_files * 8) / 10;


	// Try calculating minimum and recommended number of files:
	ssize_t new_database_pool_size;
	ssize_t new_max_clients;
	ssize_t files = 1;
	ssize_t minimum_files = 1;
	ssize_t recommended_files = 1;

	while (true) {
		ssize_t used_files = FDS_RESERVED;

		used_files += FDS_PER_DATABASE;
		new_database_pool_size = (files - used_files) / FDS_PER_DATABASE;
		if (new_database_pool_size > opts.database_pool_size) {
			new_database_pool_size = opts.database_pool_size;
		}
		used_files += (new_database_pool_size + 1) * FDS_PER_DATABASE;

		used_files += FDS_PER_CLIENT;
		new_max_clients = (files - used_files) / FDS_PER_CLIENT;
		if (new_max_clients > opts.max_clients) {
			new_max_clients = opts.max_clients;
		}
		used_files += (new_max_clients + 1) * FDS_PER_CLIENT;

		if (new_database_pool_size < 1 || new_max_clients < 1) {
			files = minimum_files = used_files;
		} else if (new_database_pool_size < opts.database_pool_size || new_max_clients < opts.max_clients) {
			files = recommended_files = used_files;
		} else {
			break;
		}
	}


	// Calculate max_files (from configuration, recommended and available numbers):
	ssize_t max_files = opts.max_files;
	if (max_files != 0) {
		if (max_files > aprox_available_files) {
			L_WARNING_ONCE("The requested open files limit of {} {} the system-wide currently available number of files: {}", max_files, max_files > available_files ? "exceeds" : "almost exceeds", available_files);
		}
	} else {
		max_files = recommended_files;
		if (max_files > aprox_available_files) {
			L_WARNING_ONCE("The minimum recommended open files limit of {} {} the system-wide currently available number of files: {}", max_files, max_files > available_files ? "exceeds" : "almost exceeds", available_files);
		}
	}


	// Try getting current limit of files:
	ssize_t limit_cur_files;
	struct rlimit limit;
	if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
		limit_cur_files = available_files;
		if ((limit_cur_files == 0) || limit_cur_files > 4000) {
			limit_cur_files = 4000;
		}
		L_WARNING_ONCE("Unable to obtain the current NOFILE limit, assuming {}: {} ({}): {}", limit_cur_files, error::name(errno), errno, error::description(errno));
	} else {
		limit_cur_files = limit.rlim_cur;
	}


	// Set the the max number of files:
	// Increase if the current limit is not enough for our needs or
	// decrease if the user requests it.
	if ((opts.max_files != 0) || limit_cur_files < max_files) {
		bool increasing = limit_cur_files < max_files;

		const ssize_t step = 16;
		int setrlimit_errno = 0;

		// Try to set the file limit to match 'max_files' or at least to the higher value supported less than max_files.
		ssize_t new_max_files = max_files;
		while (new_max_files != limit_cur_files) {
			limit.rlim_cur = static_cast<rlim_t>(new_max_files);
			limit.rlim_max = static_cast<rlim_t>(new_max_files);
			if (setrlimit(RLIMIT_NOFILE, &limit) != -1) {
				if (increasing) {
					L_INFO("Increased maximum number of open files to {} (it was originally set to {})", new_max_files, (std::size_t)limit_cur_files);
				} else {
					L_INFO("Decresed maximum number of open files to {} (it was originally set to {})", new_max_files, (std::size_t)limit_cur_files);
				}
				break;
			}

			// We failed to set file limit to 'new_max_files'. Try with a smaller limit decrementing by a few FDs per iteration.
			setrlimit_errno = errno;
			if (!increasing || new_max_files < step) {
				// Assume that the limit we get initially is still valid if our last try was even lower.
				new_max_files = limit_cur_files;
				break;
			}
			new_max_files -= step;
		}

		if (setrlimit_errno != 0) {
			L_ERR("Server can't set maximum open files to {} because of OS error: {} ({}): {}", max_files, error::name(setrlimit_errno), setrlimit_errno, error::description(setrlimit_errno));
		}
		max_files = new_max_files;
	} else {
		max_files = limit_cur_files;
	}


	// Calculate database_pool_size and max_clients from current max_files:
	files = max_files;
	ssize_t used_files = FDS_RESERVED;
	used_files += FDS_PER_DATABASE;
	new_database_pool_size = (files - used_files) / FDS_PER_DATABASE;
	if (new_database_pool_size > opts.database_pool_size) {
		new_database_pool_size = opts.database_pool_size;
	}
	used_files += (new_database_pool_size + 1) * FDS_PER_DATABASE;
	used_files += FDS_PER_CLIENT;
	new_max_clients = (files - used_files) / FDS_PER_CLIENT;
	if (new_max_clients > opts.max_clients) {
		new_max_clients = opts.max_clients;
	}


	// Warn about changes to the configured database_pool_size or max_clients:
	if (new_database_pool_size > 0 && new_database_pool_size < opts.database_pool_size) {
		L_WARNING_ONCE("You requested a database_pool_size of {} requiring at least {} max file descriptors", opts.database_pool_size, (opts.database_pool_size + 1) * FDS_PER_DATABASE  + FDS_RESERVED);
		L_WARNING_ONCE("Current maximum open files is {} so database_pool_size has been reduced to {} to compensate for low limit.", max_files, new_database_pool_size);
	}
	if (new_max_clients > 0 && new_max_clients < opts.max_clients) {
		L_WARNING_ONCE("You requested max_clients of {} requiring at least {} max file descriptors", opts.max_clients, (opts.max_clients + 1) * FDS_PER_CLIENT + FDS_RESERVED);
		L_WARNING_ONCE("Current maximum open files is {} so max_clients has been reduced to {} to compensate for low limit.", max_files, new_max_clients);
	}


	// Warn about minimum/recommended sizes:
	if (max_files < minimum_files) {
		L_CRIT("Your open files limit of {} is not enough for the server to start. Please increase your system-wide open files limit to at least {}",
			max_files,
			minimum_files);
		L_WARNING_ONCE("If you need to increase your system-wide open files limit use 'ulimit -n'");
		throw SystemExit(EX_OSFILE);
	} else if (max_files < recommended_files) {
		L_WARNING_ONCE("Your current max_files of {} is not enough. Please increase your system-wide open files limit to at least {}",
			max_files,
			recommended_files);
		L_WARNING_ONCE("If you need to increase your system-wide open files limit use 'ulimit -n'");
	}


	// Set new values:
	opts.max_files = max_files;
	opts.database_pool_size = new_database_pool_size;
	opts.max_clients = new_max_clients;
}


void demote(const char* username, const char* group) {
	// lose root privileges if we have them
	uid_t uid = getuid();

	if (
		uid == 0
		#ifdef HAVE_SETRESUID
		// Get full root privileges. Normally being effectively root is enough,
		// but some security modules restrict actions by processes that are only
		// effectively root. To make sure we don't hit those problems, we try
		// to switch to root fully.
		|| setresuid(0, 0, 0) == 0
		#endif
		|| geteuid() == 0
	) {
		if (username == nullptr || *username == '\0') {
			L_CRIT("Can't run as root without the --uid switch");
			throw SystemExit(EX_USAGE);
		}

		// Get the target user:
		struct passwd *pw;
		if ((pw = getpwnam(username)) == nullptr) {
			uid = std::atoi(username);
			if ((uid == 0u) || (pw = getpwuid(uid)) == nullptr) {
				L_CRIT("Can't find the user {} to switch to", username);
				throw SystemExit(EX_NOUSER);
			}
		}
		uid = pw->pw_uid;
		gid_t gid = pw->pw_gid;
		username = pw->pw_name;

		// Get the target group:
		struct group *gr;
		if ((group != nullptr) && (*group != 0)) {
			if ((gr = getgrnam(group)) == nullptr) {
				gid = std::atoi(group);
				if ((gid == 0u) || (gr = getgrgid(gid)) == nullptr) {
					L_CRIT("Can't find the group {} to switch to", group);
					throw SystemExit(EX_NOUSER);
				}
			}
			gid = gr->gr_gid;
			group = gr->gr_name;
		} else {
			if ((gr = getgrgid(gid)) == nullptr) {
				L_CRIT("Can't find the group id {}", gid);
				throw SystemExit(EX_NOUSER);
			}
			group = gr->gr_name;
		}

		#ifdef HAVE_SYS_CAPABILITY_H
		// Create an empty set of capabilities.
		cap_t capabilities = cap_init();

		// Capabilities have three subsets:
		//      INHERITABLE:    Capabilities permitted after an execv()
		//      EFFECTIVE:      Currently effective capabilities
		//      PERMITTED:      Limiting set for the two above.
		// See man 7 capabilities for details, Thread Capability Sets.
		//
		// We need the following capabilities:
		//      CAP_SYS_NICE    For nice(2), setpriority(2),
		//                      sched_setscheduler(2), sched_setparam(2),
		//                      sched_setaffinity(2), etc.
		//      CAP_SETUID      For setuid(), setresuid()
		// in the last two subsets. We do not need to retain any capabilities
		// over an exec().
		cap_value_t root_caps[2] = { CAP_SYS_NICE, CAP_SETUID };
		if (cap_set_flag(capabilities, CAP_PERMITTED, sizeof root_caps / sizeof root_caps[0], root_caps, CAP_SET) ||
			cap_set_flag(capabilities, CAP_EFFECTIVE, sizeof root_caps / sizeof root_caps[0], root_caps, CAP_SET)) {
			L_CRIT("Cannot manipulate capability data structure as root: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}

		// Above, we just manipulated the data structure describing the flags,
		// not the capabilities themselves. So, set those capabilities now.
		if (cap_set_proc(capabilities)) {
			L_CRIT("Cannot set capabilities as root: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}

		// We wish to retain the capabilities across the identity change,
		// so we need to tell the kernel.
		if (prctl(PR_SET_KEEPCAPS, 1L)) {
			L_CRIT("Cannot keep capabilities after dropping privileges: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}
		#endif

		// Drop extra privileges (aside from capabilities) by switching
		// to the target group and user:
		if (setgid(gid) < 0 || setuid(uid) < 0) {
			L_CRIT("Failed to assume identity of {}:{}", username, group);
			throw SystemExit(EX_OSERR);
		}

		#ifdef HAVE_SYS_CAPABILITY_H
		// We can still switch to a different user due to having the CAP_SETUID
		// capability. Let's clear the capability set, except for the CAP_SYS_NICE
		// in the permitted and effective sets.
		if (cap_clear(capabilities)) {
			L_CRIT("Cannot clear capability data structure: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}

		cap_value_t user_caps[1] = { CAP_SYS_NICE };
		if (cap_set_flag(capabilities, CAP_PERMITTED, sizeof user_caps / sizeof user_caps[0], user_caps, CAP_SET) ||
			cap_set_flag(capabilities, CAP_EFFECTIVE, sizeof user_caps / sizeof user_caps[0], user_caps, CAP_SET)) {
			L_CRIT("Cannot manipulate capability data structure as user: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}

		// Apply modified capabilities.
		if (cap_set_proc(capabilities)) {
			L_CRIT("Cannot set capabilities as user: {} ({}) {}", error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSERR);
		}
		#endif

		L_NOTICE("Running as {}:{}", username, group);
	}
}


void detach() {
	pid_t pid = fork();
	if (pid != 0) {
		std::exit(EX_OK); /* parent exits */
	}
	setsid(); /* create a new session */

	/* Every output goes to /dev/null */
	int fd;
	if ((fd = io::open("/dev/null", O_RDWR, 0)) != -1) {
		io::dup2(fd, STDIN_FILENO);
		io::dup2(fd, STDOUT_FILENO);
		io::dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) { io::close(fd); }
	}
}


void writepid(const char* pidfile) {
	/* Try to write the pid file in a best-effort way. */
	int fd = io::open(pidfile, O_RDWR | O_CREAT, 0644);
	if (fd != -1) {
		auto pid = string::format("{}\n", (unsigned long)getpid());
		io::write(fd, pid.data(), pid.size());
		io::close(fd);
	}
}


void usedir(std::string_view path, bool force) {
	auto directory = normalize_path(path);
	if (string::endswith(directory, "/.xapiand")) {
		directory.resize(directory.size() - 9);
	}
	auto xapiand_directory = directory + "/.xapiand";

	if (force) {
		if (!mkdirs(xapiand_directory)) {
			L_ERR("Cannot create working directory: {}: {} ({}): {}", repr(directory), error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSFILE);
		}
	} else {
		if (!mkdir(directory) || !mkdir(xapiand_directory)) {
			L_ERR("Cannot create working directory: {}: {} ({}): {}", repr(directory), error::name(errno), errno, error::description(errno));
			throw SystemExit(EX_OSFILE);
		}

		DIR *dirp = opendir(xapiand_directory);
		if (dirp) {
			bool empty = true;
			struct dirent *ent;
			while ((ent = readdir(dirp)) != nullptr) {
				const char *s = ent->d_name;
				if (ent->d_type == DT_DIR) {
					continue;
				}
				if (ent->d_type == DT_REG) {
					if (
						std::strcmp(s, "node") == 0 ||
						std::strcmp(s, "iamchert") == 0 ||
						std::strcmp(s, "iamglass") == 0 ||
						std::strcmp(s, "iamhoney") == 0
					) {
						empty = true;
						break;
					}
				}
				empty = false;
			}
			closedir(dirp);
			if (!empty) {
				L_CRIT("Working directory must be empty or a valid Xapiand database: {}", directory);
				throw SystemExit(EX_DATAERR);
			}
		}
	}

	if (chdir(directory.c_str()) == -1) {
		L_CRIT("Cannot change current working directory to {}", directory);
		throw SystemExit(EX_OSFILE);
	}

	char buffer[PATH_MAX];
	if (getcwd(buffer, sizeof(buffer)) == nullptr) {
		L_CRIT("Cannot get current working directory");
		throw SystemExit(EX_OSFILE);
	}
	Endpoint::cwd = normalize_path(buffer, buffer, true);  // Endpoint::cwd must always end with slash
	L_NOTICE("Changed current working directory to {}", Endpoint::cwd);
}


Endpoints
resolve_index_endpoints(const Endpoint& endpoint) {
	// This function tries to resolve endpoints the "right" way (using XapiandManager)
	// but if it fails, it tries to get all available shard directories directly,
	// otherwise it uses the passed endpoint as single endpoint.
	auto endpoints = XapiandManager::resolve_index_endpoints(endpoint);
	if (endpoints.empty()) {
		auto base_path = endpoint.path + "/";
		DIR *dirp = ::opendir(base_path.c_str());
		if (dirp != nullptr) {
			struct dirent *ent;
			while ((ent = ::readdir(dirp)) != nullptr) {
				const char *n = ent->d_name;
				if (ent->d_type == DT_DIR) {
					if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'))) {
						continue;
					}
					// This is a valid directory
					if (n[0] == '.' && n[1] == '_' && n[2] == '_') {
						endpoints.add(Endpoint{base_path + n});
					}
				}
			}
			::closedir(dirp);
		}

		if (endpoints.empty()) {
			endpoints.add(endpoint);
		}
	}
	return endpoints;
}


void banner() {
	set_thread_name("MAIN");

	std::vector<std::string> values({
			string::format("Xapian v{}.{}.{}", Xapian::major_version(), Xapian::minor_version(), Xapian::revision()),
#ifdef XAPIAND_CHAISCRIPT
			string::format("ChaiScript v{}.{}", chaiscript::Build_Info::version_major(), chaiscript::Build_Info::version_minor()),
#endif
	});

	if (Logging::log_level >= LOG_NOTICE) {
		constexpr auto outer = rgb(0, 128, 0);
		constexpr auto inner = rgb(144, 238, 144);
		constexpr auto top = rgb(255, 255, 255);
		L(-LOG_NOTICE, NO_COLOR,
			"\n\n" +
			outer + "      _       "                                                       + rgb(255, 255, 255) + "      ___\n" +
			outer + "  _-´´" +      top + "_" + outer  + "``-_   "                         + rgb(255, 255, 255) + " __  /  /          _                 _\n" +
			outer + ".´ " +       top + "_-´ `-_" + outer + " `. "                         + rgb(224, 224, 224) + " \\ \\/  /__ _ _ __ (_) __ _ _ __   __| |\n" +
			outer + "| " +       top + "`-_   _-´" + outer + " | "                         + rgb(192, 192, 192) + "  \\   // _` | '_ \\| |/ _` | '_ \\ / _` |\n" +
			outer + "| " +     inner + "`-_" + top + "`-´" + inner + "_-´" + outer + " | " + rgb(160, 160, 160) + "  /   \\ (_| | |_) | | (_| | | | | (_| |\n" +
			outer + "| " +     inner + "`-_`-´_-´" + outer + " | "                         + rgb(128, 128, 128) + " / /\\__\\__,_| .__/|_|\\__,_|_| |_|\\__,_|\n" +
			outer + " `-_ " +     inner + "`-´" + outer + " _-´  "                         + rgb(96, 96, 96)    + "/_/" + rgb(144, 238, 144) + "{:^9}" + rgb(96, 96, 96) + "|_|" + rgb(144, 238, 144) + "{:^24}" + "\n" +
			outer + "    ``-´´   " + rgb(0, 128, 0) + "{:^42}" + "\n" +
					"            " + rgb(0, 96, 0)  + "{:^42}" + "\n\n",
			"v" + Package::VERSION,
			"rev:" + Package::REVISION,
			"Using " + string::join(values, ", ", " and "),
			"[" + Package::BUGREPORT + "]");
	}

	L(-LOG_NOTICE, NOTICE_COL, "{} (pid:{})", Package::STRING, getpid());
}


void setup() {
	// Flush threshold:
	const char *p = std::getenv("XAPIAN_FLUSH_THRESHOLD");
	if (p != nullptr) {
		L_INFO("Flush threshold is now {}. (from XAPIAN_FLUSH_THRESHOLD)", std::atoi(p));
	} else {
		if (setenv("XAPIAN_FLUSH_THRESHOLD", string::format("{}", opts.flush_threshold).c_str(), 0) == -1) {
			L_INFO("Flush threshold is 10000: {} ({}): {}", error::name(errno), errno, error::description(errno));
		} else {
			L_INFO("Flush threshold is now {}. (it was originally 10000)", opts.flush_threshold);
		}
	}

	if (opts.chert) {
		L_INFO("Using Chert databases by default.");
	} else {
		L_INFO("Using Glass databases by default.");
	}

	std::vector<std::string> modes;
	if (opts.strict) {
		modes.emplace_back("strict");
	}
	if (!modes.empty()) {
		L_INFO("Activated " + string::join(modes, ", ", " and ") + ((modes.size() == 1) ? " mode by default." : " modes by default."));
	}

	adjustOpenFilesLimit();

	usedir(opts.database, opts.force);
}


void server(std::chrono::time_point<std::chrono::system_clock> process_start) {
	if (opts.detach) {
		L_NOTICE("Xapiand is done with all work here. Daemon on process ID [{}] taking over!", getpid());
	}

	usleep(100000ULL);

	setup();

	ev::default_loop default_loop(opts.ev_flags);
	L_INFO("Connection processing backend: {}", ev_backend(default_loop.backend()));

	auto& manager = XapiandManager::make(&default_loop, opts.ev_flags, process_start);
	manager->run();

	long managers = manager.use_count() - 1;
	if (managers == 0) {
		L(-LOG_NOTICE, NOTICE_COL, "Xapiand is cleanly done with all work!");
	} else {
		L(-LOG_WARNING, WARNING_COL, "Xapiand is uncleanly done with all work ({})!\n{}", managers, manager->dump_tree());
	}
	manager.reset();
}


void dump_documents() {
	std::shared_ptr<XapiandManager> manager;
	int fd = opts.filename.empty() ? STDOUT_FILENO : io::open(opts.filename.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
	if (fd != -1) {
		try {
			setup();
			manager = XapiandManager::make();
			DatabaseHandler db_handler;
			Endpoint endpoint(opts.dump_documents);
			auto endpoints = resolve_index_endpoints(endpoint);
			L_INFO("Dumping database: {}", repr(endpoints.to_string()));
			db_handler.reset(endpoints, DB_OPEN | DB_DISABLE_WAL);
			auto sha256 = db_handler.dump_documents(fd);
			L(-LOG_NOTICE, NOTICE_COL, "Dump sha256 = {}", sha256);
			manager->join();
			manager.reset();
		} catch (...) {
			if (fd != STDOUT_FILENO) {
				io::close(fd);
			}
			if (manager) {
				manager->join();
				manager.reset();
			}
			throw;
		}
		if (fd != STDOUT_FILENO) {
			io::close(fd);
		}
	} else {
		L_CRIT("Cannot open file: {}", opts.filename);
		throw SystemExit(EX_OSFILE);
	}
}


void restore_documents() {
	std::shared_ptr<XapiandManager> manager;
	int fd = (opts.filename.empty() || opts.filename == "-") ? STDIN_FILENO : io::open(opts.filename.c_str(), O_RDONLY);
	if (fd != -1) {
		try {
			setup();
			manager = XapiandManager::make();
			DatabaseHandler db_handler;
			Endpoint endpoint(opts.restore_documents);
			auto endpoints = resolve_index_endpoints(endpoint);
			L_INFO("Restoring into: {}", repr(endpoints.to_string()));
			db_handler.reset(endpoints, DB_WRITABLE | DB_CREATE_OR_OPEN | DB_DISABLE_WAL);
			auto sha256 = db_handler.restore_documents(fd);
			L(-LOG_NOTICE, NOTICE_COL, "Restore sha256 = {}", sha256);
			manager->join();
			manager.reset();
		} catch (...) {
			if (fd != STDIN_FILENO) {
				io::close(fd);
			}
			if (manager) {
				manager->join();
				manager.reset();
			}
			throw;
		}
		if (fd != STDIN_FILENO) {
			io::close(fd);
		}
	} else {
		L_CRIT("Cannot open file: {}", opts.filename);
		throw SystemExit(EX_OSFILE);
	}
}


void
cleanup_manager()
{
	try {
		auto& manager = XapiandManager::manager(false);
		if (manager) {
			// At exit, join manager
			manager->join();
			auto sig = manager->atom_sig.load();
			if (sig < 0) {
				_Exit(-sig);
			}
		}
	} catch (const SystemExit& exc) {
		_Exit(exc.code);
	} catch (...) {
		_Exit(EX_SOFTWARE);
	}
}


int main(int argc, char **argv) {
#ifdef XAPIAND_CHECK_SIZES
	check_size();
#endif

	int exit_code = EX_OK;

	auto process_start = std::chrono::system_clock::now();

#if defined(__linux__) && !defined(__GLIBC__)
	pthread_attr_t a;
	memset(&a, 0, sizeof(pthread_attr_t));
	pthread_attr_setstacksize(&a, 8*1024*1024);  // 8MB as GLIBC
	pthread_attr_setguardsize(&a, 4096);  // one page
	pthread_setattr_default_np(&a);
#endif

	try {

		opts = parseOptions(argc, argv);

		if (opts.detach) {
			detach();
		}
		if (!opts.pidfile.empty()) {
			writepid(opts.pidfile.c_str());
		}

		atexit(cleanup_manager);

		// Initialize options:
		setup_signal_handlers();
		std::setlocale(LC_CTYPE, "");

		// Logging thread must be created after fork the parent process
		auto& handlers = Logging::handlers;
		if (opts.logfile.compare("syslog") == 0) {
			handlers.push_back(std::make_unique<SysLog>());
		} else if (!opts.logfile.empty()) {
			handlers.push_back(std::make_unique<StreamLogger>(opts.logfile.c_str()));
		}
		if (!opts.detach || handlers.empty()) {
			handlers.push_back(std::make_unique<StderrLogger>());
		}

		Logging::log_level += opts.verbosity;
		Logging::colors = opts.colors;
		Logging::no_colors = opts.no_colors;

		demote(opts.uid.c_str(), opts.gid.c_str());

#ifdef XAPIAN_HAS_GLASS_BACKEND
		if (!opts.chert) {
			// Prefer glass database
			if (setenv("XAPIAN_PREFER_GLASS", "1", 0) != 0) {
				opts.chert = true;
			}
		}
#endif

		if (opts.strict) {
			default_spc.flags.strict = true;
		}

		banner();

		try {
			if (!opts.dump_documents.empty()) {
				dump_documents();
			} else if (!opts.restore_documents.empty()) {
				restore_documents();
			} else {
				server(process_start);
			}
		} catch (const SystemExit& exc) {
			exit_code = exc.code;
		} catch (const BaseException& exc) {
			L_CRIT("Uncaught exception: {}", *exc.get_context() ? exc.get_context() : "Unkown BaseException!");
			exit_code = EX_SOFTWARE;
		} catch (const Xapian::Error& exc) {
			L_CRIT("Uncaught exception: {}", exc.get_description());
			exit_code = EX_SOFTWARE;
		} catch (const std::exception& exc) {
			L_CRIT("Uncaught exception: {}", *exc.what() ? exc.what() : "Unkown std::exception!");
			exit_code = EX_SOFTWARE;
		} catch (...) {
			L_CRIT("Uncaught exception!");
			exit_code = EX_SOFTWARE;
		}

	} catch (const SystemExit& exc) {
		exit_code = exc.code;
	} catch (...) {
		L_CRIT("Uncaught exception!!");
		exit_code = EX_SOFTWARE;
	}

	if (!opts.pidfile.empty()) {
		L_INFO("Removing the pid file.");
		unlink(opts.pidfile.c_str());
	}

	Logging::finish();
	Logging::join();

	return exit_code;
}
