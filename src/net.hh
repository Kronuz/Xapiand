/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <cstring>               // for strerror
#include <netinet/in.h>          // for IPPROTO_TCP
#include <netinet/tcp.h>         // for TCP_NOPUSH
#include <string>                // for std::string

#include "io.hh"                 // for io::setsockopt
#include "log.h"                 // for L_ERR
#include "string.hh"             // for string::format, string::join


static inline std::string fast_inet_ntop4(const struct in_addr& addr) {
	// char ip[INET_ADDRSTRLEN];
	// inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
	// return std::string(ip);
	return string::format("%d.%d.%d.%d",
		addr.s_addr & 0xff,
		(addr.s_addr >> 8) & 0xff,
		(addr.s_addr >> 16) & 0xff,
		(addr.s_addr >> 24) & 0xff);
}


static inline void tcp_nopush(int sock) {
	int optval = 1;

#ifdef TCP_NOPUSH
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

#ifdef TCP_CORK
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_CORK (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif
}


static inline void tcp_push(int sock) {
	int optval = 0;

#ifdef TCP_NOPUSH
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

#ifdef TCP_CORK
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_CORK (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif
}
