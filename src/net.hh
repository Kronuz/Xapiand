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

#pragma once

#include <errno.h>                  // for errno
#include <netinet/in.h>             // for IPPROTO_TCP
#include <netinet/tcp.h>            // for TCP_NOPUSH
#include <string>                   // for std::string

#include "cassert.h"                // for ASSERT
#include "error.hh"                 // for error:name, error::description
#include "io.hh"                    // for io::setsockopt
#include "log.h"                    // for L_ERR
#include "string.hh"                // for string::format, string::join


inline std::string inet_ntop(const struct sockaddr_in& addr) {
	// char ip[INET_ADDRSTRLEN] = {};
	// if (inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr) {
	// 	L_ERR("ERROR: inet_ntop: %s (%d): %s", error::name(errno), errno, error::description(errno));
	// }
	// return std::string(ip);
	return string::format("{}.{}.{}.{}",
		addr.sin_addr.s_addr & 0xff,
		(addr.sin_addr.s_addr >> 8) & 0xff,
		(addr.sin_addr.s_addr >> 16) & 0xff,
		(addr.sin_addr.s_addr >> 24) & 0xff);
}


inline void tcp_nopush(int sock) {
	int optval = 1;

#ifdef TCP_NOPUSH
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
	}
#endif

#ifdef TCP_CORK
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_CORK {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
	}
#endif
}


inline void tcp_push(int sock) {
	int optval = 0;

#ifdef TCP_NOPUSH
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
	}
#endif

#ifdef TCP_CORK
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_CORK {sock:%d}: %s (%d): %s", sock, error::name(errno), errno, error::description(errno));
	}
#endif
}
