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

#include <stdlib.h>


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


void run(int num_servers, const char *cluster_name_, const char *discovery_group, int discovery_port, int http_port, int binary_port)
{
	ev::default_loop default_loop;

	setup_signal_handlers();

	XapiandManager manager(&default_loop, cluster_name_, discovery_group, discovery_port, http_port, binary_port);

	manager_ptr = &manager;

	manager.run(num_servers);

	manager_ptr = NULL;
}


int main(int argc, char **argv)
{
	int num_servers = 8;
	int discovery_port = 0;
	int http_port = 0;
	int binary_port = 0;
	const char *discovery_group = NULL;
	const char *cluster_name = "xapiand";
	const char *p;

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

	INFO((void *)NULL, "Joined cluster: %s\n", cluster_name);

	// Prefer glass database
	if (setenv("XAPIAN_PREFER_GLASS", "1", false) == 0) {
		INFO((void *)NULL, "Enabled glass database.\n");
	}

	// Enable changesets
	if (setenv("XAPIAN_MAX_CHANGESETS", "10", false) == 0) {
		INFO((void *)NULL, "Database changesets set to 10.\n");
	}

	// Flush threshold increased
	int flush_threshold = 10000;  // Default is 10000 (if no set)
	p = getenv("XAPIAN_FLUSH_THRESHOLD");
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

	if (argc > 2) {
		http_port = atoi(argv[1]);
		binary_port = atoi(argv[2]);
	}

	run(num_servers, cluster_name, discovery_group, discovery_port, http_port, binary_port);
	INFO((void *)NULL, "Done with all work!\n");

	return 0;
}
