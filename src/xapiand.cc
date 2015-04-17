/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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


void run(int num_servers, int http_port, int binary_port)
{
	ev::default_loop default_loop;
	
	setup_signal_handlers();
	
	XapiandManager manager(&default_loop, http_port, binary_port);

	manager_ptr = &manager;
	
	manager.run(num_servers);
	
	manager_ptr = NULL;
}

int main(int argc, char **argv)
{
	int num_servers = 8;
	int http_port = XAPIAND_HTTP_SERVERPORT;
	int binary_port = XAPIAND_BINARY_SERVERPORT;

	if (argc > 2) {
		http_port = atoi(argv[1]);
		binary_port = atoi(argv[2]);
	}

	LOG((void *)NULL, "Starting %s (%s).\n", PACKAGE_STRING, PACKAGE_BUGREPORT);
	run(num_servers, http_port, binary_port);
	LOG((void *)NULL, "Done with all work!\n");

	return 0;
}
