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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_PTHREAD_SETNAME_NP_2
#include <pthread_np.h>
#endif


using namespace TCLAP;


XapiandManager *manager_ptr = NULL;


static void sig_shutdown_handler(int sig)
{
	if (manager_ptr) {
		manager_ptr->sig_shutdown_handler(sig);
	}
}


void setup_signal_handlers(void) {
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	struct sigaction act;

	/* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
	 * Otherwise, sa_handler is used. */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_shutdown_handler;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
}

// int num_servers, const char *cluster_name_, const char *node_name_, const char *discovery_group, int discovery_port, int http_port, int binary_port, size_t dbpool_size
void run(const opts_t &opts)
{
#ifdef HAVE_PTHREAD_SETNAME_NP_3
	pthread_setname_np(pthread_self(), "==");
#elif HAVE_PTHREAD_SETNAME_NP_2
	pthread_set_name_np(pthread_self(),"==");
#else
	pthread_setname_np("==");
#endif

	ev::default_loop default_loop;

	setup_signal_handlers();

	XapiandManager manager(&default_loop, opts);

	manager_ptr = &manager;

	manager.run((int)opts.num_servers, 3);  // FIXME: make replicators an option

	manager_ptr = NULL;
}


// This exemplifies how the output class can be overridden to provide
// user defined output.
class CmdOutput : public StdOutput
{
	public:
		virtual void failure(CmdLineInterface& _cmd, ArgException& e) {
			std::string progName = _cmd.getProgramName();

			std::cerr << "Error: " << e.argId() << std::endl;

			spacePrint(std::cerr, e.error(), 75, 3, 0);

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

			throw ExitException(1);
		}

		virtual void usage(CmdLineInterface& _cmd) {
			spacePrint(std::cout, PACKAGE_STRING, 75, 0, 0);
			spacePrint(std::cout, "[" PACKAGE_BUGREPORT "]", 75, 0, 0);

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


void parseOptions(int argc, char** argv, opts_t &opts)
{
	unsigned int nthreads = std::thread::hardware_concurrency() * 2;

	try {
		CmdLine cmd("", ' ', PACKAGE_STRING);

		// ZshCompletionOutput zshoutput;
		// cmd.setOutput(&zshoutput);

		CmdOutput output;
		cmd.setOutput(&output);

		MultiSwitchArg verbosity("v", "verbose", "Increase verbosity.", cmd);
		SwitchArg daemonize("d", "daemon", "daemonize (run in background).", cmd);

#ifdef XAPIAN_HAS_GLASS_BACKEND
		SwitchArg chert("", "chert", "Use chert databases.", cmd, false);
#endif

		ValueArg<std::string> database("D", "database", "Node database.", false, ".", "path", cmd);
		ValueArg<std::string> cluster_name("", "cluster", "Cluster name to join.", false, XAPIAND_CLUSTER_NAME, "cluster", cmd);
		ValueArg<std::string> node_name("n", "name", "Node name.", false, "", "node", cmd);

		ValueArg<unsigned int> http_port("", "http", "HTTP REST API port", false, XAPIAND_HTTP_SERVERPORT, "port", cmd);
		ValueArg<unsigned int> binary_port("", "xapian", "Xapian binary protocol port", false, XAPIAND_BINARY_SERVERPORT, "port", cmd);
		ValueArg<unsigned int> discovery_port("", "discovery", "Discovery UDP port", false, XAPIAND_DISCOVERY_SERVERPORT, "port", cmd);
		ValueArg<std::string> discovery_group("", "group", "Discovery UDP group", false, XAPIAND_DISCOVERY_GROUP, "group", cmd);

		ValueArg<std::string> pidfile("p", "pid", "Write PID to <pidfile>.", false, "xapiand.pid", "pidfile", cmd);
		ValueArg<std::string> uid("u", "uid", "User ID.", false, "xapiand", "uid", cmd);
		ValueArg<std::string> gid("g", "gid", "Group ID.", false, "xapiand", "uid", cmd);


		ValueArg<size_t> num_servers("", "workers", "Number of worker servers.", false, nthreads, "threads", cmd);
		ValueArg<size_t> dbpool_size("", "dbpool", "Maximum number of database endpoints in database pool.", false, 1000, "size", cmd);


		cmd.parse( argc, argv );

		opts.verbosity = verbosity.getValue();
		opts.daemonize = daemonize.getValue();
#ifdef XAPIAN_HAS_GLASS_BACKEND
		opts.chert = chert.getValue();
#else
		opts.chert = true;
#endif
		opts.database = database.getValue();
		opts.cluster_name = cluster_name.getValue();
		opts.node_name = node_name.getValue();
		opts.http_port = http_port.getValue();
		opts.binary_port = binary_port.getValue();
		opts.discovery_port = discovery_port.getValue();
		opts.discovery_group = discovery_group.getValue();
		opts.pidfile = pidfile.getValue();
		opts.uid = uid.getValue();
		opts.gid = gid.getValue();
		opts.num_servers = num_servers.getValue();
		opts.dbpool_size = dbpool_size.getValue();
		opts.threadpool_size = 10;
		opts.endpoints_list_size = 10;

		// Use 0 for guessing with defaults
		if (opts.http_port == XAPIAND_HTTP_SERVERPORT) opts.http_port = 0;
		if (opts.binary_port == XAPIAND_BINARY_SERVERPORT) opts.binary_port = 0;
		if (opts.discovery_port == XAPIAND_DISCOVERY_SERVERPORT) opts.discovery_port = 0;
		if (opts.discovery_group.empty()) opts.discovery_group = XAPIAND_DISCOVERY_GROUP;

	} catch (ArgException &e) { // catch any exceptions
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
	}
}


void daemonize(void) {
	int fd;

	if (fork() != 0) exit(0); /* parent exits */
	setsid(); /* create a new session */

	/* Every output goes to /dev/null */
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) close(fd);
	}
}


int main(int argc, char **argv)
{
	opts_t opts;

	parseOptions(argc, argv, opts);

	INFO((void *)NULL,
		"\n\n"
		"  __  __           _                 _\n"
		"  \\ \\/ /__ _ _ __ (_) __ _ _ __   __| |\n"
		"   \\  // _` | '_ \\| |/ _` | '_ \\ / _` |\n"
		"   /  \\ (_| | |_) | | (_| | | | | (_| |\n"
		"  /_/\\_\\__,_| .__/|_|\\__,_|_| |_|\\__,_|\n"
		"            |_|  v%s\n"
		"   [%s]\n"
		"          Using Xapian v%s\n\n", PACKAGE_VERSION, PACKAGE_BUGREPORT, XAPIAN_VERSION);

#ifdef XAPIAN_HAS_GLASS_BACKEND
	if (!opts.chert) {
		// Prefer glass database
		if (setenv("XAPIAN_PREFER_GLASS", "1", false) != 0) {
			opts.chert = true;
		}
	}
#endif

	if (opts.chert) {
		INFO((void *)NULL, "By default using Chert databases.\n");
	} else {
		INFO((void *)NULL, "By default using Glass databases.\n");
	}

	// Enable changesets
	if (setenv("XAPIAN_MAX_CHANGESETS", "200", false) == 0) {
		INFO((void *)NULL, "Database changesets set to 200.\n");
	}

	// Flush threshold increased
	int flush_threshold = 10000;  // Default is 10000 (if no set)
	const char *p = getenv("XAPIAN_FLUSH_THRESHOLD");
	if (p) flush_threshold = atoi(p);
	if (flush_threshold < 100000 && setenv("XAPIAN_FLUSH_THRESHOLD", "100000", false) == 0) {
		INFO((void *)NULL, "Increased flush threshold to 100000 (it was originally set to %d).\n", flush_threshold);
	}

	time(&init_time);
	struct tm *timeinfo = localtime(&init_time);
	timeinfo->tm_hour   = 0;
	timeinfo->tm_min    = 0;
	timeinfo->tm_sec    = 0;
	int diff_t = (int)(init_time - mktime(timeinfo));
	b_time.minute = diff_t / SLOT_TIME_SECOND;
	b_time.second =  diff_t % SLOT_TIME_SECOND;

	if (opts.daemonize) {
		daemonize();
	}

	run(opts);

	INFO((void *)NULL, "Done with all work!\n");
	return 0;
}
