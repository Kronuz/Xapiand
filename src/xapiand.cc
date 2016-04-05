/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "xapiand.h"

#include "utils.h"
#include "manager.h"

#include "tclap/CmdLine.h"
#include "tclap/ZshCompletionOutput.h"

#include <thread>
#include <clocale>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sysexits.h>  // EX_*
#include <dirent.h>    // opendir, readdir, DIR, struct dirent
#include <fcntl.h>

#define LINE_LENGTH 78


using namespace TCLAP;


void setup_signal_handlers(void) {
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	struct sigaction act;

	/* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
	 * Otherwise, sa_handler is used. */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_exit;
	sigaction(SIGTERM, &act, nullptr);
	sigaction(SIGINT, &act, nullptr);
}


/*
 * This exemplifies how the output class can be overridden to provide
 * user defined output.
 */
class CmdOutput : public StdOutput {
	inline void spacePrint(std::ostream& os, const std::string& s, int maxWidth,
				int indentSpaces, int secondLineOffset, bool endl=true) const {
		int len = static_cast<int>(s.length());

		if ((len + indentSpaces > maxWidth) && maxWidth > 0) {
			int allowedLen = maxWidth - indentSpaces;
			int start = 0;
			while (start < len) {
				// find the substring length
				// int stringLen = std::min<int>( len - start, allowedLen );
				// doing it this way to support a VisualC++ 2005 bug
				using namespace std;
				int stringLen = min<int>(len - start, allowedLen);

				// trim the length so it doesn't end in middle of a word
				if (stringLen == allowedLen) {
					while (stringLen >= 0 &&
						s[stringLen+start] != ' ' &&
						s[stringLen+start] != ',' &&
						s[stringLen+start] != '|') {
						--stringLen;
					}
				}

				// ok, the word is longer than the line, so just split
				// wherever the line ends
				if (stringLen <= 0) {
					stringLen = allowedLen;
				}

				// check for newlines
				for (int i = 0; i < stringLen; ++i) {
					if (s[start+i] == '\n') {
						stringLen = i + 1;
					}
				}

				if (start != 0) {
					os << std::endl;
				}

				// print the indent
				for (int i = 0; i < indentSpaces; ++i) {
					os << " ";
				}

				if (start == 0) {
					// handle second line offsets
					indentSpaces += secondLineOffset;

					// adjust allowed len
					allowedLen -= secondLineOffset;
				}

				os << s.substr(start,stringLen);

				// so we don't start a line with a space
				while (s[stringLen+start] == ' ' && start < len) {
					++start;
				}

				start += stringLen;
			}
		} else {
			for (int i = 0; i < indentSpaces; ++i) {
				os << " ";
			}
			os << s;
			if (endl) {
				os << std::endl;
			}
		}
	}

	inline void _shortUsage(CmdLineInterface& _cmd, std::ostream& os) const {
		std::list<Arg*> argList = _cmd.getArgList();
		std::string progName = _cmd.getProgramName();
		XorHandler xorHandler = _cmd.getXorHandler();
		std::vector<std::vector<Arg*>> xorList = xorHandler.getXorList();

		std::string s = progName + " ";

		// first the xor
		for (size_t i = 0; i < xorList.size(); ++i) {
			s += " {";
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				s += (*it)->shortID() + "|";
			}

			s[s.length() - 1] = '}';
		}

		// then the rest
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				s += " " + (*it)->shortID();
			}
		}

		// if the program name is too long, then adjust the second line offset
		int secondLineOffset = static_cast<int>(progName.length()) + 2;
		if (secondLineOffset > LINE_LENGTH / 2) {
			secondLineOffset = static_cast<int>(LINE_LENGTH / 2);
		}

		spacePrint(os, s, LINE_LENGTH, 3, secondLineOffset);
	}

	inline void _longUsage(CmdLineInterface& _cmd, std::ostream& os) const {
		std::list<Arg*> argList = _cmd.getArgList();
		std::string message = _cmd.getMessage();
		XorHandler xorHandler = _cmd.getXorHandler();
		std::vector< std::vector<Arg*> > xorList = xorHandler.getXorList();

		size_t max = 0;

		for (size_t i = 0; i < xorList.size(); ++i) {
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				const std::string& id = (*it)->longID();
				if (id.size() > max) {
					max = id.size();
				}
			}
		}

		// first the xor
		for (size_t i = 0; i < xorList.size(); ++i) {
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				const std::string& id = (*it)->longID();
				spacePrint(os, id, (int)max + 3, 3, 3, false);
				spacePrint(os, (*it)->getDescription(), LINE_LENGTH - ((int)max - 10), static_cast<int>((2 + max) - id.size()), static_cast<int>(3 + id.size()), false);

				if (it + 1 != xorList[i].end()) {
					spacePrint(os, "-- OR --", LINE_LENGTH, 9, 0);
				}
			}
			os << std::endl << std::endl;
		}

		// then the rest
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				const std::string& id = (*it)->longID();
				if (id.size() > max) {
					max = id.size();
				}
			}
		}

		// then the rest
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				const std::string& id = (*it)->longID();
				spacePrint(os, id, (int)max + 3, 3, 3, false);
				spacePrint(os, (*it)->getDescription(), LINE_LENGTH - ((int)max - 10), static_cast<int>((2 + max) - id.size()), static_cast<int>(3 + id.size()), false);
				os << std::endl;
			}
		}

		os << std::endl;

		if (!message.empty()) {
			spacePrint( os, message, LINE_LENGTH, 3, 0 );
		}
	}

public:
	virtual void failure(CmdLineInterface& _cmd, ArgException& exc) {
		std::string progName = _cmd.getProgramName();

		std::cerr << "Error: " << exc.argId() << std::endl;

		spacePrint(std::cerr, exc.error(), LINE_LENGTH, 3, 0);

		std::cerr << std::endl;

		if (_cmd.hasHelpAndVersion()) {
			std::cerr << "Usage: " << std::endl;

			_shortUsage( _cmd, std::cerr );

			std::cerr << std::endl << "For complete usage and help type: "
					  << std::endl << "   " << progName << " "
					  << Arg::nameStartString() << "help"
					  << std::endl << std::endl;
		} else {
			usage(_cmd);
		}

		throw ExitException(EX_USAGE);
	}

	virtual void usage(CmdLineInterface& _cmd) {
		spacePrint(std::cout, PACKAGE_STRING, LINE_LENGTH, 0, 0);
		spacePrint(std::cout, "[" PACKAGE_BUGREPORT "]", LINE_LENGTH, 0, 0);

		std::cout << std::endl;

		std::cout << "Usage: " << std::endl;

		_shortUsage(_cmd, std::cout);

		std::cout << std::endl << "Where: " << std::endl;

		_longUsage(_cmd, std::cout);
	}

	virtual void version(CmdLineInterface& _cmd) {
		std::string xversion = _cmd.getVersion();
		std::cout << xversion << std::endl;
	}
};


void parseOptions(int argc, char** argv, opts_t &opts) {
	const unsigned int nthreads = std::thread::hardware_concurrency() * SERVERS_MULTIPLIER;

	try {
		CmdLine cmd("", ' ', PACKAGE_STRING);

		// ZshCompletionOutput zshoutput;
		// cmd.setOutput(&zshoutput);

		CmdOutput output;
		cmd.setOutput(&output);

		SwitchArg detach("d", "detach", "detach process. (run in background)", cmd);
		MultiSwitchArg verbose("v", "verbose", "Increase verbosity.", cmd);
		ValueArg<unsigned int> verbosity("", "verbosity", "Set verbosity.", false, 0, "verbosity", cmd);

#ifdef XAPIAN_HAS_GLASS_BACKEND
		SwitchArg chert("", "chert", "Prefer Chert databases.", cmd, false);
#endif

		SwitchArg solo("", "solo", "Run solo indexer. (no replication or discovery)", cmd, false);

		ValueArg<std::string> database("D", "database", "Path to the root of the node.", false, ".", "path", cmd);
		ValueArg<std::string> cluster_name("", "cluster", "Cluster name to join.", false, XAPIAND_CLUSTER_NAME, "cluster", cmd);
		ValueArg<std::string> node_name("", "name", "Node name.", false, "", "node", cmd);

		ValueArg<unsigned int> http_port("", "http", "TCP HTTP port number to listen on for REST API.", false, XAPIAND_HTTP_SERVERPORT, "port", cmd);
		ValueArg<unsigned int> binary_port("", "xapian", "Xapian binary protocol TCP port number to listen on.", false, XAPIAND_BINARY_SERVERPORT, "port", cmd);

		ValueArg<unsigned int> discovery_port("", "discovery", "Discovery UDP port number to listen on.", false, XAPIAND_DISCOVERY_SERVERPORT, "port", cmd);
		ValueArg<std::string> discovery_group("", "dgroup", "Discovery UDP group name.", false, XAPIAND_DISCOVERY_GROUP, "group", cmd);

		ValueArg<unsigned int> raft_port("", "raft", "Raft UDP port number to listen on.", false, XAPIAND_RAFT_SERVERPORT, "port", cmd);
		ValueArg<std::string> raft_group("", "rgroup", "Raft UDP group name.", false, XAPIAND_RAFT_GROUP, "group", cmd);

		ValueArg<std::string> pidfile("P", "pidfile", "Save PID in <file>.", false, "", "file", cmd);
		ValueArg<std::string> logfile("L", "logfile", "Save logs in <file>.", false, "", "file", cmd);
		ValueArg<std::string> uid("", "uid", "User ID.", false, "", "uid", cmd);
		ValueArg<std::string> gid("", "gid", "Group ID.", false, "", "gid", cmd);

		ValueArg<size_t> num_servers("", "workers", "Number of worker servers.", false, nthreads, "threads", cmd);
		ValueArg<size_t> dbpool_size("", "dbpool", "Maximum number of databases in database pool.", false, DBPOOL_SIZE, "size", cmd);
		ValueArg<size_t> num_replicators("", "replicators", "Number of replicators.", false, NUM_REPLICATORS, "replicators", cmd);
		ValueArg<size_t> num_committers("", "committers", "Number of committers.", false, NUM_COMMITTERS, "committers", cmd);

		std::vector<std::string> args;
		for (int i = 0; i < argc; ++i) {
			if (i == 0) {
				const char* a = strrchr(argv[i], '/');
				if (a) {
					++a;
				} else {
					a = argv[i];
				}
				args.push_back(a);
			} else {
				// Split arguments when possible (e.g. -Dnode, --verbosity=3)
				const char* arg = argv[i];
				if (arg[0] == '-') {
					if (arg[1] != '-' && arg[1] != 'v') {  // skip long arguments (e.g. --verbosity) or multiswitch (e.g. -vvv)
						std::string tmp(arg, 2);
						args.push_back(tmp);
						arg += 2;
					}
				}
				const char* a = strchr(arg, '=');
				if (a) {
					if (a - arg) {
						std::string tmp(arg, a - arg);
						args.push_back(tmp);
					}
					arg = a + 1;
				}
				if (*arg) {
					args.push_back(arg);
				}
			}
		}
		cmd.parse(args);

		opts.verbosity = verbosity.getValue() + verbose.getValue();
		opts.detach = detach.getValue();
#ifdef XAPIAN_HAS_GLASS_BACKEND
		opts.chert = chert.getValue();
#else
		opts.chert = true;
#endif

		opts.solo = solo.getValue();

		opts.database = database.getValue();
		opts.cluster_name = cluster_name.getValue();
		opts.node_name = node_name.getValue();
		opts.http_port = http_port.getValue();
		opts.binary_port = binary_port.getValue();
		opts.discovery_port = discovery_port.getValue();
		opts.discovery_group = discovery_group.getValue();
		opts.raft_port = raft_port.getValue();
		opts.raft_group = raft_group.getValue();
		opts.pidfile = pidfile.getValue();
		opts.logfile = logfile.getValue();
		opts.uid = uid.getValue();
		opts.gid = gid.getValue();
		opts.num_servers = num_servers.getValue();
		opts.dbpool_size = dbpool_size.getValue();
		opts.num_replicators = opts.solo ? 0 : num_replicators.getValue();
		opts.num_committers = num_committers.getValue();
		opts.threadpool_size = THEADPOOL_SIZE;
		opts.endpoints_list_size = ENDPOINT_LIST_SIZE;
		if (opts.detach) {
			if (opts.logfile.empty()) {
				opts.logfile = XAPIAND_LOG_FILE;
			}
			if (opts.pidfile.empty()) {
				opts.pidfile = XAPIAND_PID_FILE;
			}
		}
		opts.ev_flags = ev::AUTO;

	} catch (const ArgException& exc) { // catch any exceptions
		std::cerr << "error: " << exc.error() << " for arg " << exc.argId() << std::endl;
	}
}


void demote(const char* username, const char* group) {
	/* lose root privileges if we have them */
	uid_t uid = getuid();
	if (uid == 0 || geteuid() == 0) {
		gid_t gid = getgid();
		if (username == nullptr || *username == '\0') {
			L_CRIT(nullptr, "Can't run as root without the --uid switch");
			throw Exit(EX_USAGE);

		}

		struct passwd *pw;
		struct group *gr;

		if ((pw = getpwnam(username)) == nullptr) {
			uid = atoi(username);
			if (!uid || (pw = getpwuid(uid)) == nullptr) {
				L_CRIT(nullptr, "Can't find the user %s to switch to", username);
				throw Exit(EX_NOUSER);
			}
		}
		uid = pw->pw_uid;
		gid = pw->pw_gid;
		username = pw->pw_name;

		if (group && *group) {
			if ((gr = getgrnam(group)) == nullptr) {
				gid = atoi(group);
				if (!gid || (gr = getgrgid(gid)) == nullptr) {
					L_CRIT(nullptr, "Can't find the group %s to switch to", group);
					throw Exit(EX_NOUSER);
				}
			}
			gid = gr->gr_gid;
			group = gr->gr_name;
		} else {
			if ((gr = getgrgid(gid)) == nullptr) {
				L_CRIT(nullptr, "Can't find the group id %d", gid);
				throw Exit(EX_NOUSER);
			}
			group = gr->gr_name;
		}
		if (setgid(gid) < 0 || setuid(uid) < 0) {
			L_CRIT(nullptr, "Failed to assume identity of %s:%s", username, group);
			throw Exit(EX_OSERR);
		}
		L_NOTICE(nullptr, "Running as %s:%s", username, group);
	}
}


void detach() {
	pid_t pid = fork();
	if (pid != 0) {
		L_NOTICE(nullptr, "Xapiand is done with all work here. Daemon on process ID [%d] taking over!", pid);
		exit(EX_OK); /* parent exits */
	}
	setsid(); /* create a new session */

	/* Every output goes to /dev/null */
	int fd;
	if ((fd = io::open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) io::close(fd);
	}
}


void writepid(const char* pidfile) {
	if (pidfile != nullptr && *pidfile != '\0') {
		/* Try to write the pid file in a best-effort way. */
		int fd = io::open(pidfile, O_RDWR | O_CREAT, 0644);
		if (fd > 0) {
			char buffer[100];
			snprintf(buffer, sizeof(buffer), "%lu\n", (unsigned long)getpid());
			io::write(fd, buffer, strlen(buffer));
			io::close(fd);
		}
	}
}


void usedir(const char* path) {
	DIR *dirp;
	dirp = opendir(path, true);
	if (!dirp) {
		L_CRIT(nullptr, "Cannot open working directory: %s", path);
		throw Exit(EX_OSFILE);
	}

	bool empty = true;
	struct dirent *ent;
	while ((ent = readdir(dirp)) != nullptr) {
		const char *s = ent->d_name;
		if (ent->d_type == DT_DIR) {
			continue;
		}
		if (ent->d_type == DT_REG) {
#if defined(__APPLE__) && defined(__MACH__)
			if (ent->d_namlen == 9 && strcmp(s, "flintlock") == 0)
#else
			if (strcmp(s, "flintlock") == 0)
#endif
			{
				empty = true;
				break;
			}
		}
		empty = false;
	}
	closedir(dirp);

	if (!empty) {
		L_CRIT(nullptr, "Working directory must be empty or a valid xapian database: %s", path);
		throw Exit(EX_DATAERR);
	}

	if (chdir(path) == -1) {
		L_CRIT(nullptr, "Cannot change current working directory to %s", path);
		throw Exit(EX_OSFILE);
	}

	char buffer[PATH_MAX];
	getcwd(buffer, sizeof(buffer));
	strcat(buffer, "/");  // Endpoint::cwd must always end with slash
	Endpoint::cwd = normalize_path(buffer, buffer);
	L_NOTICE(nullptr, "Changed current working directory to %s", Endpoint::cwd.c_str());
}


void banner() {
	set_thread_name("===");
	L_INFO(nullptr,
		"\n\n" WHITE
		"  __  __           _                 _\n"
		"  \\ \\/ /__ _ _ __ (_) __ _ _ __   __| |\n"
		"   \\  // _` | '_ \\| |/ _` | '_ \\ / _` |\n"
		"   /  \\ (_| | |_) | | (_| | | | | (_| |\n"
		"  /_/\\_\\__,_| .__/|_|\\__,_|_| |_|\\__,_|\n"
		"            |_|  " BRIGHT_GREEN "v%s\n" GREEN
		"   [%s]\n"
		"          Using Xapian v%s\n\n", PACKAGE_VERSION, PACKAGE_BUGREPORT, XAPIAN_VERSION);
}


#define EV_SELECT_NAME  "select"
#define EV_POLL_NAME    "poll"
#define EV_EPOLL_NAME   "epoll"
#define EV_KQUEUE_NAME  "kqueue"
#define EV_DEVPOLL_NAME "devpoll"
#define EV_PORT_NAME    "port"

const char*
ev_backend(unsigned int backend)
{
	switch(backend) {
		case ev::SELECT:
			return EV_SELECT_NAME;
		case ev::POLL:
			return EV_POLL_NAME;
		case ev::EPOLL:
			return EV_EPOLL_NAME;
		case ev::KQUEUE:
			return EV_KQUEUE_NAME;
		case ev::DEVPOLL:
			return EV_DEVPOLL_NAME;
		case ev::PORT:
			return EV_PORT_NAME;
	}
	return "unknown";
}

std::string
ev_supported()
{
	std::string backends;
	unsigned int supported = ev::supported_backends();
	if (supported & ev::SELECT) backends += std::string(", ") + EV_SELECT_NAME;
	if (supported & ev::POLL) backends += std::string(", ") + EV_POLL_NAME;
	if (supported & ev::EPOLL) backends += std::string(", ") + EV_EPOLL_NAME;
	if (supported & ev::KQUEUE) backends += std::string(", ") + EV_KQUEUE_NAME;
	if (supported & ev::DEVPOLL) backends += std::string(", ") + EV_DEVPOLL_NAME;
	if (supported & ev::PORT) backends += std::string(", ") + EV_PORT_NAME;
	if (backends.empty()) {
		return "unknown";
	} else {
		backends.erase(0, 2);
	}
	return backends;
}


void run(const opts_t &opts) {
	usedir(opts.database.c_str());

	demote(opts.uid.c_str(), opts.gid.c_str());

	init_time = std::chrono::system_clock::now();
	time_t epoch = std::chrono::system_clock::to_time_t(init_time);
	struct tm *timeinfo = localtime(&epoch);
	timeinfo->tm_hour   = 0;
	timeinfo->tm_min    = 0;
	timeinfo->tm_sec    = 0;
	auto diff_t = epoch - mktime(timeinfo);
	b_time.minute = diff_t / SLOT_TIME_SECOND;
	b_time.second =  diff_t % SLOT_TIME_SECOND;

	setup_signal_handlers();
	ev::default_loop default_loop(opts.ev_flags);

	L_INFO(nullptr, "libev backend: %s (available: %s)", ev_backend(default_loop.backend()), ev_supported().c_str());

	XapiandManager::manager = Worker::make_shared<XapiandManager>(&default_loop, opts.ev_flags, opts);
	try {
		XapiandManager::manager->run(opts);
	} catch (...) {
		XapiandManager::manager.reset();
		throw;
	}

	int managers = static_cast<int>(XapiandManager::manager.use_count());
	if (managers == 1) {
		L_NOTICE(nullptr, "Xapiand is cleanly done with all work!");
	} else {
		L_WARNING(nullptr, "Xapiand is uncleanly done with all work (%d)!", managers);
	}
	XapiandManager::manager.reset();
}


int main(int argc, char **argv) {
	opts_t opts;

	parseOptions(argc, argv, opts);

	std::setlocale(LC_CTYPE, "");

	if (opts.logfile.compare("syslog") == 0) {
		Log::handlers.push_back(std::make_unique<SysLog>());
	} else if (!opts.logfile.empty()) {
		Log::handlers.push_back(std::make_unique<StreamLogger>(opts.logfile.c_str()));
	}
	if (!opts.detach) {
		Log::handlers.push_back(std::make_unique<StderrLogger>());
	}

	Log::log_level += opts.verbosity;

	banner();
	if (opts.detach) {
		detach();
		writepid(opts.pidfile.c_str());
		banner();
	}
	L_NOTICE(nullptr, "Xapiand started.");

#ifdef XAPIAN_HAS_GLASS_BACKEND
	if (!opts.chert) {
		// Prefer glass database
		if (setenv("XAPIAN_PREFER_GLASS", "1", false) != 0) {
			opts.chert = true;
		}
	}
#endif

	if (opts.chert) {
		L_INFO(nullptr, "Using Chert databases by default.");
	} else {
		L_INFO(nullptr, "Using Glass databases by default.");
	}

	// Flush threshold increased
	int flush_threshold = 10000;  // Default is 10000 (if no set)
	const char *p = getenv("XAPIAN_FLUSH_THRESHOLD");
	if (p) flush_threshold = atoi(p);
	if (flush_threshold < 100000 && setenv("XAPIAN_FLUSH_THRESHOLD", "100000", false) == 0) {
		L_INFO(nullptr, "Increased flush threshold to 100000 (it was originally set to %d).", flush_threshold);
	}

	try {
		run(opts);
	} catch (const Exit& exc) {
		if (opts.detach && !opts.pidfile.empty()) {
			L_INFO(nullptr, "Removing the pid file.");
			unlink(opts.pidfile.c_str());
		}

		Log::finish(true);
		return exc.code;
	}

	if (opts.detach && !opts.pidfile.empty()) {
		L_INFO(nullptr, "Removing the pid file.");
		unlink(opts.pidfile.c_str());
	}

	Log::finish(true);
	return EX_OK;
}
