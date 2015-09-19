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

#include "utils.h"

#include <string>
#include <cstdlib>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <pthread.h>
#include <xapian.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <netdb.h> /* for getaddrinfo */
#include <unistd.h>
#include <dirent.h>
#include <bitset>


#define COORDS_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d+|\\d+)"
#define NUMERIC_RE "-?(\\d*\\.\\d+|\\d+)"
#define FIND_RANGE_RE "([^ ]*)\\.\\.([^ ]*)"

#define STATE_ERR -1
#define STATE_CM0 0
#define STATE_CMD 1
#define STATE_NSP 2
#define STATE_PTH 3
#define STATE_HST 4

pthread_mutex_t qmtx = PTHREAD_MUTEX_INITIALIZER;

pcre *compiled_coords_re = NULL;
pcre *compiled_numeric_re = NULL;
pcre *compiled_find_range_re = NULL;

pos_time_t b_time;
time_t init_time;
times_row_t stats_cnt;


std::string repr(const char * p, size_t size, bool friendly)
{
	char *buff = new char[size * 4 + 1];
	char *d = buff;
	const char *p_end = p + size;
	while (p != p_end) {
		char c = *p++;
		if (friendly && c == 9) {
			*d++ = '\\';
			*d++ = 't';
		} else if (friendly && c == 10) {
			*d++ = '\\';
			*d++ = 'n';
		} else if (friendly && c == 13) {
			*d++ = '\\';
			*d++ = 'r';
		} else if (friendly && c == '\'') {
			*d++ = '\\';
			*d++ = '\'';
		} else if (friendly && c >= ' ' && c <= '~') {
			*d++ = c;
		} else {
			*d++ = '\\';
			*d++ = 'x';
			sprintf(d, "%02x", (unsigned char)c);
			d += 2;
		}
		//printf("%02x: %ld < %ld\n", (unsigned char)c, (unsigned long)(d - buff), (unsigned long)(size * 4 + 1));
	}
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


std::string repr(const std::string &string, bool friendly)
{
	return repr(string.c_str(), string.size(), friendly);
}


void log(const char *file, int line, void *obj, const char *format, ...)
{
	pthread_mutex_lock(&qmtx);

	char name[100];
#ifdef HAVE_PTHREAD_SETNAME_NP_2

#else
	pthread_getname_np(pthread_self(), name, sizeof(name));
#endif
	// fprintf(stderr, "tid(0x%012lx:%2s): 0x%012lx - %s:%d - ", (unsigned long)thread, name, (unsigned long)obj, file, line);
	fprintf(stderr, "tid(%2s): ../%s:%d: ", *name ? name : "--", file, line);
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);

	pthread_mutex_unlock(&qmtx);
}


void check_tcp_backlog(int tcp_backlog)
{
#if defined(NET_CORE_SOMAXCONN)
	int name[3] = {CTL_NET, NET_CORE, NET_CORE_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(NULL, "ERROR: sysctl: [%d] %s\n", errno, strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(NULL, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"net.core.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#elif defined(KIPC_SOMAXCONN)
	int name[3] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(NULL, "ERROR: sysctl: [%d] %s\n", errno, strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(NULL, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"kern.ipc.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#endif
}


int bind_tcp(const char *type, int &port, struct sockaddr_in &addr, int tries)
{
	int sock;

	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int optval = 1;
	// struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(NULL, "ERROR: %s socket: [%d] %s\n", type, errno, strerror(errno));
		return -1;
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_REUSEADDR (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}
#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: %s setsockopt TCP_NODELAY (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	// }

	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: %s setsockopt SO_LINGER (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	// }

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(NULL, "ERROR: %s bind error (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
		} else {
			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
				LOG_ERR(NULL, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
			}
			check_tcp_backlog(tcp_backlog);
			listen(sock, tcp_backlog);
			return sock;
		}
	}

	LOG_ERR(NULL, "ERROR: %s bind error (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	close(sock);
	return -1;
}


int bind_udp(const char *type, int &port, struct sockaddr_in &addr, int tries, const char *group)
{
	int sock;

	int optval = 1;
	unsigned char ttl = 3;
	struct ip_mreq mreq;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		LOG_ERR(NULL, "ERROR: %s socket: [%d] %s\n", type, errno, strerror(errno));
		return -1;
	}

	// use setsockopt() to allow multiple listeners connected to the same port
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_REUSEPORT (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_MULTICAST_LOOP (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_MULTICAST_TTL (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_ADD_MEMBERSHIP (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
		close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to all addresses (differs from sender)

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(NULL, "ERROR: %s bind error (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
		} else {
			fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
			addr.sin_addr.s_addr = inet_addr(group);  // setup s_addr for sender (send to group)
			return sock;
		}
	}

	LOG_ERR(NULL, "ERROR: %s bind error (sock=%d): [%d] %s\n", type, sock, errno, strerror(errno));
	close(sock);
	return -1;
}


int connection_socket()
{
	int sock;

	int optval = 1;
	// struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(NULL, "ERROR: cannot create binary connection: [%d] %s\n", errno, strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt TCP_NODELAY (sock=%d): %s\n", sock, strerror(errno));
	// }

	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt SO_LINGER (sock=%d): %s\n", sock, strerror(errno));
	// }

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
		LOG_ERR(NULL, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
	}

	return sock;
}

int connect_tcp(int sock, const char *hostname, const char *servname)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_protocol = 0;

	struct addrinfo *result;
	if (getaddrinfo(hostname, servname, &hints, &result) < 0) {
		LOG_ERR(NULL, "Couldn't resolve host %s:%s\n", hostname, servname);
		close(sock);
		return -1;
	}

	if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
		if (!ignored_errorno(errno, false)) {
			LOG_ERR(NULL, "Cannot connect to %s:%s (sock=%d): [%d] %s\n", hostname, servname, sock, errno, strerror(errno));
			freeaddrinfo(result);
			close(sock);
			return -1;
		}
	}

	freeaddrinfo(result);

	return sock;
}


int accept_tcp(int listener_sock)
{
	int sock;

	int optval = 1;

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((sock = accept(listener_sock, (struct sockaddr *)&addr, &addrlen)) < 0) {
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: setsockopt SO_NOSIGPIPE (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt TCP_NODELAY (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
	// }

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
		LOG_ERR(NULL, "ERROR: fcntl O_NONBLOCK (sock=%d): [%d] %s\n", sock, errno, strerror(errno));
	}

	return sock;
}


int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets)
{
	/* It outputs a bucket number in the range [0, num_buckets).
	   A Fast, Minimal Memory, Consistent Hash Algorithm
	   [http://arxiv.org/pdf/1406.2294v1.pdf] */
	int64_t b = 1, j = 0;
	while (j < num_buckets) {
		b = j;
		key = key * 2862933555777941757ULL + 1;
		j = (b + 1) * (double(1LL << 31) / double((key >> 33) + 1));
	}
	return (int32_t) b;
}


const char * name_prefix[] = {
	"",
	"bil", "bal", "ban",
	"hil", "ham", "hal", "hol", "hob",
	"wil", "me", "or", "ol", "od",
	"gor", "for", "fos", "tol",
	"ar", "fin", "ere",
	"leo", "vi", "bi", "bren", "thor",
};


const char * name_stem[] = {
	"",
	"go", "orbis", "apol", "adur", "mos", "ri", "i",
	"na", "ole", "n",
};


const char * name_suffix[] = {
	"",
	"tur", "axia", "and", "bo", "gil", "bin",
	"bras", "las", "mac", "grim", "wise", "l",
	"lo", "fo", "co",
	"ra", "via", "da", "ne",
	"ta",
	"y",
	"wen", "thiel", "phin", "dir", "dor", "tor", "rod", "on",
	"rdo", "dis",
};


std::string name_generator()
{
	std::string name;

	while (name.size() < 4) {
		// Add the prefix...
		name.append(name_prefix[(rand() % (sizeof(name_prefix) / sizeof(const char *)))]);

		// Add the stem...
		name.append(name_stem[(rand() % (sizeof(name_stem) / sizeof(const char *)))]);

		// Add the suffix...
		name.append(name_suffix[(rand() % (sizeof(name_suffix) / sizeof(const char *)))]);
	}

	// Make the first letter capital...
	name[0] = toupper(name[0]);

	return name;
}


const char HEX2DEC[256] =
{
	/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
	/* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

	/* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};


std::string urldecode(const char *src, size_t size)
{
	// Note from RFC1630:  "Sequences which start with a percent sign
	// but are not followed by two hexadecimal characters (0-9, A-F) are reserved
	// for future extension"

	const char * SRC_END = src + size;
	const char * SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

	char * const pStart = new char[size];
	char * pEnd = pStart;

	while (src < SRC_LAST_DEC)
	{
		if (*src == '%')
		{
			char dec1, dec2;
			if (-1 != (dec1 = HEX2DEC[*(src + 1)])
				&& -1 != (dec2 = HEX2DEC[*(src + 2)]))
			{
				*pEnd++ = (dec1 << 4) + dec2;
				src += 3;
				continue;
			}
		}

		*pEnd++ = *src++;
	}

	// the last 2- chars
	while (src < SRC_END)
	*pEnd++ = *src++;

	std::string sResult(pStart, pEnd);
	delete [] pStart;
	//std::replace( sResult.begin(), sResult.end(), '+', ' ');
	return sResult;
}


int url_qs(const char *name, const char *qs, size_t size, parser_query_t *par)
{
	const char *nf = qs + size;
	const char *n1, *n0;
	const char *v0 = NULL;

	if (par->offset == NULL) {
		n0 = n1 = qs;
	} else {
		n0 = n1 = par->offset + par->length;
	}

	while (1) {
		char cn = *n1;
		if (n1 == nf) {
			cn = '\0';
		}
		switch (cn) {
			case '=' :
			v0 = n1;
			case '\0':
			case '&' :
			case ';' :
			if(strlen(name) == n1 - n0 && strncmp(n0, name, n1 - n0) == 0) {
				if (v0) {
					const char *v1 = v0 + 1;
					while (1) {
						char cv = *v1;
						if (v1 == nf) {
							cv = '\0';
						}
						switch(cv) {
							case '\0':
							case '&' :
							case ';' :
							par->offset = v0 + 1;
							par->length = v1 - v0 - 1;
							return 0;
						}
						v1++;
					}
				} else {
					par->offset = n1 + 1;
					par->length = 0;
					return 0;
				}
			} else if (!cn) {
				return -1;
			} else if (cn != '=') {
				n0 = n1 + 1;
				v0 = NULL;
			}
		}
		n1++;
	}
	return -1;
}


int url_path(const char* ni, size_t size, parser_url_path_t *par)
{
	const char *nf = ni + size;
	const char *n0, *n1, *n2 = NULL;
	int state, direction;
	size_t length;

	if (par->offset == NULL) {
		state = STATE_CM0;
		n0 = n1 = n2 = nf - 1;
		direction = -1;
	} else {
		state = STATE_NSP;
		n0 = n1 = n2 = par->offset;
		nf = par->off_command - 1;
		direction = 1;
	}

	while (state != STATE_ERR) {
		if (!(n1 >= ni && n1 <= nf)) {
			return -1;
		}

		char cn = (n1 >= nf) ? '\0' : *n1;
		switch(cn) {
			case '\0':
				if (n0 == n1) return -1;

			case ',':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_NSP:
					case STATE_PTH:
						length = n1 - n0;
						par->off_path = n0;
						par->len_path = length;
						state = length ? 0 : STATE_ERR;
						if (cn) n1++;
						par->offset = n1;
						return state;
					case STATE_HST:
						length = n1 - n0;
						par->off_host = n0;
						par->len_host = length;
						state = length ? 0 : STATE_ERR;
						if (cn) n1++;
						par->offset = n1;
						return state;
				}
				break;

			case ':':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_NSP:
						length = n1 - n0;
						par->off_namespace = n0;
						par->len_namespace = length;
						state = length ? STATE_PTH : STATE_ERR;
						n0 = n1 + 1;
						break;
					case STATE_HST:
						break;
					default:
						state = STATE_ERR;
				}
				break;

			case '@':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_NSP:
						length = n1 - n0;
						par->off_path = n0;
						par->len_path = length;
						state = length ? STATE_HST : STATE_ERR;
						n0 = n1 + 1;
						break;
					case STATE_PTH:
						par->off_path = n0;
						par->len_path = n1 - n0;
						state = STATE_HST;
						n0 = n1 + 1;
						break;
					default:
						state = STATE_ERR;
				}
				break;

			case '/':
				switch (state) {
					case STATE_CM0:
						break;
					case STATE_CMD:
						length = n0 - n1;
						par->off_command = n1 + 1;
						par->len_command = length;
						state = length ? STATE_NSP : STATE_ERR;
						nf = n1;
						n0 = n1 = n2 = ni;
						direction = 1;
						par->offset = n0;
						break;
				}
				break;

			default:
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
				}
				break;
		}
		n1 += direction;
	}
	return -1;
}


int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, unique_group &unique_groups)
{
	int erroffset;
	const char *error;

	// First, the regex string must be compiled.
	if (*code == NULL) {
		// pcre_free is not used after compiling the regular expression here because
		// it's compiled into a global static variable, which gets freed by the end of the program.
		*code = pcre_compile(pattern, 0, &error, &erroffset, 0);
		if (*code == NULL) {
			LOG_ERR(NULL, "pcre_compile of %s failed (offset: %d), %s\n", pattern, erroffset, error);
			return -1;
		}
	}

	if (*code != NULL) {
		int n;
		if (pcre_fullinfo(*code, NULL, PCRE_INFO_CAPTURECOUNT, &n) != 0) {
			return -1;
		}

		if (unique_groups == NULL) {
			unique_groups = unique_group(static_cast<group_t*>(malloc((n + 1) * 3 * sizeof(int))));
			if (unique_groups == NULL) {
				LOG_ERR(NULL, "Memory can not be reserved\n");
				return -1;
			}
		}

		int *ocvector = (int*)unique_groups.get();
		if (pcre_exec(*code, 0, subject, length, startoffset, options, ocvector, (n + 1) * 3) >= 0) {
			return 0;
		} else return -1;
	}

	return -1;
}


std::string stringtoupper(const std::string &str)
{
	std::string tmp = str;
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM_UPPER());
	return tmp;
}


std::string stringtolower(const std::string &str)
{
	std::string tmp = str;
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM_LOWER());
	return tmp;
}


std::string prefixed(const std::string &term, const std::string &prefix)
{
	if (isupper(term.at(0))) {
		return (prefix.empty()) ? term : prefix + ":" + term;
	}

	return prefix + term;
}


unsigned int get_slot(const std::string &name)
{
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? stringtoupper(name) : name), 24, 8);
	unsigned int slot = hex2int(_md5);
	if (slot == 0x00000000) {
		slot = 0x00000001; // 0->id
	} else if (slot == Xapian::BAD_VALUENO) {
		slot = 0xfffffffe;
	}
	return slot;
}


unsigned int hex2int(const std::string &input)
{
	unsigned int n;
	std::stringstream ss;
	ss << std::hex << input;
	ss >> n;
	ss.flush();
	return n;
}


int strtoint(const std::string &str)
{
	int number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


unsigned int strtouint(const std::string &str)
{
	unsigned int number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


double strtodouble(const std::string &str)
{
	double number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


long long int strtollong(const std::string &str)
{
	long long int number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


uInt64 strtouInt64(const std::string &str)
{
	uInt64 number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


std::string get_prefix(const std::string &name, const std::string &prefix, char type)
{
	std::string slot(get_slot_hex(name));
	std::transform(slot.begin(), slot.end(), slot.begin(), TRANSFORM_MAP());
	std::string res(prefix);
	res.append(1, toupper(type));
	return res + slot;
}


std::string get_slot_hex(const std::string &name)
{
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? stringtoupper(name): name), 24, 8);
	return stringtoupper(_md5);
}


bool strhasupper(const std::string &str)
{
	std::string::const_iterator it(str.begin());
	for ( ; it != str.end(); it++) {
		if (isupper(*it)) return true;
	}

	return false;
}


bool isRange(const std::string &str, unique_group &unique_gr)
{
	int ret = pcre_search(str.c_str(), (int)str.size(), 0, 0, FIND_RANGE_RE, &compiled_find_range_re, unique_gr);

	return (ret != -1) ? true : false;
}


bool isRange(const std::string &str)
{
	unique_group unique_gr;
	int ret = pcre_search(str.c_str(), (int)str.size(), 0, 0, FIND_RANGE_RE, &compiled_find_range_re, unique_gr);

	return (ret != -1) ? true : false;
}


bool isNumeric(const std::string &str)
{
	unique_group unique_gr;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, NUMERIC_RE, &compiled_numeric_re, unique_gr);
	group_t *g = unique_gr.get();

	return (ret != -1 && (g[0].end - g[0].start) == len) ? true : false;
}


bool startswith(const std::string &text, const std::string &token)
{
	if (text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}


void update_pos_time()
{
	time_t t_current;
	time(&t_current);
	unsigned short aux_second = b_time.second;
	unsigned short aux_minute = b_time.minute;
	unsigned int t_elapsed = (unsigned int)(t_current - init_time);
	if (t_elapsed < SLOT_TIME_SECOND) {
		b_time.second += t_elapsed;
		if (b_time.second >= SLOT_TIME_SECOND) {
			b_time.minute += b_time.second / SLOT_TIME_SECOND;
			b_time.second = b_time.second % SLOT_TIME_SECOND;
			fill_zeros_stats_sec(aux_second + 1, SLOT_TIME_SECOND -1);
			fill_zeros_stats_sec(0, b_time.second);
		} else {
			fill_zeros_stats_sec(aux_second + 1, b_time.second);
		}
	} else {
		b_time.second = t_elapsed % SLOT_TIME_SECOND;
		fill_zeros_stats_sec(0, SLOT_TIME_SECOND - 1);
		b_time.minute += t_elapsed / SLOT_TIME_SECOND;
	}
	init_time = t_current;
	if (b_time.minute >= SLOT_TIME_MINUTE) {
		b_time.minute = b_time.minute % SLOT_TIME_MINUTE;
		fill_zeros_stats_cnt(aux_minute + 1, SLOT_TIME_MINUTE - 1);
		fill_zeros_stats_cnt(0, b_time.minute);
	} else {
		fill_zeros_stats_cnt(aux_minute + 1, b_time.minute);
	}
}


void fill_zeros_stats_cnt(int start, int end)
{
	for (int i = start; i <= end; ++i) {
		stats_cnt.index.cnt[i] = 0;
		stats_cnt.index.tm_cnt[i] = 0;
		stats_cnt.search.cnt[i] = 0;
		stats_cnt.search.tm_cnt[i] = 0;
		stats_cnt.del.cnt[i] = 0;
		stats_cnt.del.tm_cnt[i] = 0;
	}
}


void fill_zeros_stats_sec(int start, int end)
{
	for (int i = start; i <= end; ++i) {
		stats_cnt.index.sec[i] = 0;
		stats_cnt.index.tm_sec[i] = 0;
		stats_cnt.search.sec[i] = 0;
		stats_cnt.search.tm_sec[i] = 0;
		stats_cnt.del.sec[i] = 0;
		stats_cnt.del.tm_sec[i] = 0;
	}
}


void delete_files(const std::string &path)
{
	unsigned char isFile = 0x8;
	unsigned char isFolder = 0x4;

	bool contains_folder = false;
	DIR *Dir;

	struct dirent *Subdir;
	Dir = opendir(path.c_str());

	if (!Dir) {
		return;
	}

	Subdir = readdir(Dir);
	while (Subdir) {
		if (Subdir->d_type == isFolder) {
			if (strcmp(Subdir->d_name, ".") != 0 && strcmp(Subdir->d_name, "..") != 0) {
				contains_folder = true;
			}
		}

		if (Subdir->d_type == isFile) {
			std::string file = path + "/" + std::string(Subdir->d_name);
			if (remove(file.c_str()) != 0) {
				LOG_ERR(NULL, "File %s could not be deleted\n", Subdir->d_name);
			}
		}
		Subdir = readdir(Dir);
	}

	if (!contains_folder) {
		if (rmdir(path.c_str()) != 0) {
			LOG_ERR(NULL, "Directory %s could not be deleted\n", path.c_str());
		}
	}
}


void move_files(const std::string &src, const std::string &dst)
{
	unsigned char isFile = 0x8;

	DIR *Dir;
	struct dirent *Subdir;
	Dir = opendir(src.c_str());

	if (!Dir) {
		return;
	}

	Subdir = readdir(Dir);
	while (Subdir) {
		if (Subdir->d_type == isFile) {
			std::string old_name = src + "/" + Subdir->d_name;
			std::string new_name = dst + "/" + Subdir->d_name;
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				LOG_ERR(NULL, "Couldn't rename %s to %s\n", old_name.c_str(), new_name.c_str());
			}
		}
		Subdir = readdir(Dir);
	}

	if (rmdir(src.c_str()) != 0) {
		LOG_ERR(NULL, "Directory %s could not be deleted\n", src.c_str());
	}
}


void stringTokenizer(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens)
{
	size_t prev = 0, next = 0, len;

	while ((next = str.find(delimiter, prev)) != std::string::npos) {
		len = next - prev;
		if (len > 0) {
			tokens.push_back(str.substr(prev, len));
		}
		prev = next + delimiter.size();
	}

	if (prev < str.size()) {
		tokens.push_back(str.substr(prev));
	}
}


unsigned int levenshtein_distance(const std::string &str1, const std::string &str2)
{
	const size_t len1 = str1.size(), len2 = str2.size();
	std::vector<unsigned int> col(len2 + 1), prev_col(len2 + 1);

	for (unsigned int i = 0; i < prev_col.size(); i++) prev_col[i] = i;

	for (unsigned int i = 0; i < len1; i++) {
		col[0] = i + 1;
		for (unsigned int j = 0; j < len2; j++)
			col[j + 1] = std::min(std::min(prev_col[j + 1] + 1, col[j] + 1), prev_col[j] + (str1[i] == str2[j] ? 0 : 1));
		col.swap(prev_col);
	}

	return prev_col[len2];
}