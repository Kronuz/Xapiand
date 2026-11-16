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

/*
 * Fault-injection hooks for the `io` library, wired to Xapiand.
 *
 * The io library turns every operation into a potential failure point through two
 * macros that are no-ops by default. This header is the library's
 * IO_RANDOM_ERRORS_HEADER: under XAPIAND_RANDOM_ERRORS it restores Xapiand's
 * resiliency-testing behavior -- each op independently fails with probability
 * opts.random_errors_io / opts.random_errors_net (net failures also shutdown the
 * socket first, as the in-tree engine did). With XAPIAND_RANDOM_ERRORS off this
 * header defines nothing, so io.hh keeps its zero-overhead no-op defaults.
 */

#pragma once

#ifdef XAPIAND_RANDOM_ERRORS

#include <sys/socket.h>             // for ::shutdown, SHUT_RDWR

#include "random.hh"                // for random_real
#include "opts.h"                   // for opts

#define RANDOM_ERRORS_IO_ERRNO_RETURN(errnum) \
	if (opts.random_errors_io) { \
		auto prob = random_real(0, 1); \
		if (prob < opts.random_errors_io) { \
			errno = errnum; \
			return -1; \
		} \
	}
#define RANDOM_ERRORS_NET_ERRNO_RETURN(errnum, sock) \
	if (opts.random_errors_net) { \
		auto prob = random_real(0, 1); \
		if (prob < opts.random_errors_net) { \
			if (sock) { \
				::shutdown(sock, SHUT_RDWR); \
			} \
			errno = errnum; \
			return -1; \
		} \
	}

#endif
