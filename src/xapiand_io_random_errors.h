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
