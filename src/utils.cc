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
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <netdb.h> /* for getaddrinfo */
#include <unistd.h>
#include <dirent.h>


#define DATE_RE "([1-9][0-9]{3})([-/ ]?)(0[1-9]|1[0-2])\\2(0[0-9]|[12][0-9]|3[01])([T ]?([01]?[0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])([.,]([0-9]{1,3}))?)?([ ]*[+-]([01]?[0-9]|2[0-3]):([0-5][0-9])|Z)?)?([ ]*\\|\\|[ ]*([+-/\\dyMwdhms]+))?"
#define DATE_MATH_RE "([+-]\\d+|\\/{1,2})([dyMwhms])"
#define COORDS_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d+|\\d+)"
#define COORDS_DISTANCE_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d*|\\d+)\\s?;\\s?(\\d*\\.\\d*|\\d+)(ft|in|yd|mi|km|[m]{1,2}|cm)?"
#define NUMERIC_RE "(\\d*\\.\\d+|\\d+)"
#define FIND_RANGE_RE "([^ ]*\\.\\.)"
#define FIND_ORDER_RE "([_a-zA-Z][_a-zA-Z0-9]+,[_a-zA-Z][_a-zA-Z0-9]*)"
#define RANGE_ID_RE "(\\d+)\\s?..\\s?(\\d*)"

#define STATE_ERR -1
#define STATE_CM0 0
#define STATE_CMD 1
#define STATE_NSP 2
#define STATE_PTH 3
#define STATE_HST 4

pthread_mutex_t qmtx = PTHREAD_MUTEX_INITIALIZER;

pcre *compiled_date_re = NULL;
pcre *compiled_date_math_re = NULL;
pcre *compiled_coords_re = NULL;
pcre *compiled_coords_dist_re = NULL;
pcre *compiled_numeric_re = NULL;
pcre *compiled_find_range_re = NULL;
pcre *compiled_range_id_re = NULL;

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

	FILE * file_ = stderr;
	char name[100];
#ifdef HAVE_PTHREAD_SETNAME_NP_2

#else
	pthread_getname_np(pthread_self(), name, sizeof(name));
#endif
	// fprintf(file_, "tid(0x%012lx:%2s): 0x%012lx - %s:%d - ", (unsigned long)thread, name, (unsigned long)obj, file, line);
	fprintf(file_, "tid(%2s): ../%s:%d: ", *name ? name : "--", file, line);
	va_list argptr;
	va_start(argptr, format);
	vfprintf(file_, format, argptr);
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
		LOG_ERR(NULL, "ERROR: sysctl: %s\n", strerror(errno));
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
		LOG_ERR(NULL, "ERROR: sysctl: %s\n", strerror(errno));
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
	struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(NULL, "ERROR: %s socket: %s\n", type, strerror(errno));
		return -1;
	}

	// use setsockopt() to allow multiple listeners connected to the same address
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_REUSEADDR (sock=%d): %s\n", type, sock, strerror(errno));
	}
#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_NOSIGPIPE (sock=%d): %s\n", type, sock, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: %s setsockopt TCP_NODELAY (sock=%d): %s\n", type, sock, strerror(errno));
	// }

	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: %s setsockopt SO_LINGER (sock=%d): %s\n", type, sock, strerror(errno));
	// }

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_KEEPALIVE (sock=%d): %s\n", type, sock, strerror(errno));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(NULL, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
		} else {
			if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
				LOG_ERR(NULL, "ERROR: fcntl O_NONBLOCK (sock=%d): %s\n", sock, strerror(errno));
			}
			check_tcp_backlog(tcp_backlog);
			listen(sock, tcp_backlog);
			return sock;
		}
	}

	LOG_ERR(NULL, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
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
		LOG_ERR(NULL, "ERROR: %s socket: %s\n", type, strerror(errno));
		return -1;
	}

	// use setsockopt() to allow multiple listeners connected to the same port
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt SO_REUSEPORT (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_MULTICAST_LOOP (sock=%d): %s\n", type, sock, strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_MULTICAST_TTL (sock=%d): %s\n", type, sock, strerror(errno));
	}

	// use setsockopt() to request that the kernel join a multicast group
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(group);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		LOG_ERR(NULL, "ERROR: %s setsockopt IP_ADD_MEMBERSHIP (sock=%d): %s\n", type, sock, strerror(errno));
		close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // bind to all addresses (differs from sender)

	for (int i = 0; i < tries; i++, port++) {
		addr.sin_port = htons(port);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			LOG_DEBUG(NULL, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
		} else {
			fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
			addr.sin_addr.s_addr = inet_addr(group);  // setup s_addr for sender (send to group)
			return sock;
		}
	}

	LOG_ERR(NULL, "ERROR: %s bind error (sock=%d): %s\n", type, sock, strerror(errno));
	close(sock);
	return -1;
}


int connect_tcp(const char *hostname, const char *servname)
{
	int sock;

	int optval = 1;
	struct linger ling = {0, 0};

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LOG_ERR(NULL, "ERROR: cannot create binary connection: %s\n", strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0) {
		LOG_ERR(NULL, "ERROR: setsockopt SO_NOSIGPIPE (sock=%d): %s\n", sock, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt TCP_NODELAY (sock=%d): %s\n", sock, strerror(errno));
	// }

	// if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt SO_LINGER (sock=%d): %s\n", sock, strerror(errno));
	// }

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
		LOG_ERR(NULL, "Cannot connect to %s:%d (sock=%d): %s\n", hostname, servname, sock, strerror(errno));
		freeaddrinfo(result);
		close(sock);
		return -1;
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
		LOG_ERR(NULL, "ERROR: setsockopt SO_NOSIGPIPE (sock=%d): %s\n", sock, strerror(errno));
	}
#endif
	// if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
	// 	LOG_ERR(NULL, "ERROR: setsockopt TCP_NODELAY (sock=%d): %s\n", sock, strerror(errno));
	// }

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
		LOG_ERR(NULL, "ERROR: fcntl O_NONBLOCK (sock=%d): %s\n", sock, strerror(errno));
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

	if(par->offset == NULL) {
		n0 = n1 = qs;
	} else {
		n0 = n1 = par->offset + par -> length;
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


int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, group_t **groups)
{
	int erroffset;
	const char *error;

	// First, the regex string must be compiled.
	if (*code == NULL) {
		//pcre_free is not use because we use a struct pcre static and gets free at the end of the program
		LOG(NULL, "pcre compiled is NULL.\n");
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

		if (*groups == NULL) {
			*groups = (group_t *)malloc((n + 1) * 3 * sizeof(int));
		}

		int *ocvector = (int *)*groups;
		if (pcre_exec(*code, 0, subject, length, startoffset, options, ocvector, (n + 1) * 3) >= 0) {
			return 0;
		} else return -1;
	}

	return -1;
}


std::string serialise_numeric(const std::string &field_value)
{
	double val;
	if (isNumeric(field_value)) {
		val = strtodouble(field_value);
		return Xapian::sortable_serialise(val);
	}
	return "";
}


std::string serialise_date(const std::string &field_value)
{
	std::string str_timestamp = timestamp_date(field_value);
	if (str_timestamp.size() == 0) {
		LOG_ERR(NULL, "ERROR: Format date (%s) must be ISO 8601: (eg 1997-07-16T19:20:30.451+05:00) or a epoch (double)\n", field_value.c_str());
		return "";
	}

	double timestamp = strtodouble(str_timestamp);
	LOG(NULL, "timestamp %s %f\n", str_timestamp.c_str(), timestamp);
	return Xapian::sortable_serialise(timestamp);
}


std::string unserialise_date(const std::string &serialise_val)
{
	char date[25];
	double epoch = Xapian::sortable_unserialise(serialise_val);
	time_t timestamp = (time_t) epoch;
	std::string milliseconds = std::to_string(epoch);
	milliseconds.assign(milliseconds.c_str() + milliseconds.find("."), 4);
	struct tm *timeinfo = gmtime(&timestamp);
	sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d%s", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds.c_str());
	return date;
}


std::string serialise_geo(const std::string &field_value)
{
	Xapian::LatLongCoords coords;
	double latitude, longitude;
	int len = (int) field_value.size(), Ncoord = 0, offset = 0;
	bool end = false;
	group_t *g = NULL;
	while (pcre_search(field_value.c_str(), len, offset, 0, COORDS_RE, &compiled_coords_re, &g) != -1) {
		std::string parse(field_value, g[1].start, g[1].end - g[1].start);
		latitude = strtodouble(parse);
		parse.assign(field_value, g[2].start, g[2].end - g[2].start);
		longitude = strtodouble(parse);
		Ncoord++;
		try {
			coords.append(Xapian::LatLongCoord(latitude, longitude));
		} catch (Xapian::Error &e) {
			LOG_ERR(NULL, "Latitude or longitude out-of-range\n");
			return "";
		}
		LOG(NULL, "Coord %d: %f, %f\n", Ncoord, latitude, longitude);
		if (g[2].end == len) {
			end = true;
			break;
		}
		offset = g[2].end;
	}

	if (g) {
		free(g);
		g = NULL;
	}

	if (Ncoord == 0 || !end) {
		LOG_ERR(NULL, "ERROR: %s must be an array of doubles [lat, lon, lat, lon, ...]\n", field_value.c_str());
		return "";
	}
	return coords.serialise();
}


std::string unserialise_geo(const std::string &serialise_val)
{
	Xapian::LatLongCoords coords;
	coords.unserialise(serialise_val);
	Xapian::LatLongCoordsIterator it = coords.begin();
	std::stringstream ss;
	for (; it != coords.end(); it++) {
		ss << (*it).latitude;
		ss << "," << (*it).longitude << ",";
	}
	std::string _coords = ss.str();
	return std::string(_coords, 0, _coords.size() - 1);
}


std::string serialise_bool(const std::string &field_value)
{
	if (field_value.empty()) {
		return "f";
	} else if (strcasecmp(field_value.c_str(), "true") == 0) {
		return "t";
	} else if (strcasecmp(field_value.c_str(), "false") == 0) {
		return "f";
	} else {
		return "";
	}
}


std::string stringtoupper(const std::string &str)
{
	std::string tmp = str;

	struct TRANSFORM {
		char operator() (char c) { return  toupper(c);}
	};

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM());

	return tmp;
}


std::string stringtolower(const std::string &str)
{
	std::string tmp = str;

	struct TRANSFORM {
		char operator() (char c) { return  tolower(c);}
	};

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM());

	return tmp;
}


std::string prefixed(const std::string &term, const std::string &prefix)
{
	if (isupper(term.at(0))) {
		if (prefix.size() == 0) {
			return term;
		} else {
			return prefix + ":" + term;
		}
	}

	return prefix + term;
}


unsigned int get_slot(const std::string &name)
{
	if (stringtolower(name).compare("id") == 0) return 0;

	std::string standard_name;
	if (strhasupper(name)) {
		standard_name = stringtoupper(name);
	} else {
		standard_name = name;
	}
	std::string _md5(md5(standard_name), 24, 8);
	unsigned int slot = hex2int(_md5);
	if (slot == 0x00000000) {
		slot = 0x00000001; // 0->id
	} else if (slot == 0xffffffff) {
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


double strtodouble(const std::string &str)
{
	double number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


std::string timestamp_date(const std::string &str)
{
	int len = (int) str.size();
	int ret, n[7], offset = 0;
	std::string oph, opm;
	double  timestamp;
	group_t *gr = NULL;

	ret = pcre_search(str.c_str(), len, offset, 0, DATE_RE, &compiled_date_re, &gr);

	if (ret != -1 && len == (gr[0].end - gr[0].start)) {
		std::string parse(str, gr[1].start, gr[1].end - gr[1].start);
		n[0] = strtoint(parse);
		parse.assign(str, gr[3].start, gr[3].end - gr[3].start);
		n[1] = strtoint(parse);
		parse.assign(str, gr[4].start, gr[4].end - gr[4].start);
		n[2] = strtoint(parse);
		if (gr[5].end - gr[5].start > 0) {
			parse.assign(str, gr[6].start, gr[6].end - gr[6].start);
			n[3] = strtoint(parse);
			parse.assign(str, gr[7].start, gr[7].end - gr[7].start);
			n[4] = strtoint(parse);
			if (gr[8].end - gr[8].start > 0) {
				parse.assign(str, gr[9].start, gr[9].end - gr[9].start);
				n[5] = strtoint(parse);
				if (gr[10].end - gr[10].start > 0) {
					parse.assign(str, gr[11].start, gr[11].end - gr[11].start);
					n[6] = strtoint(parse);
				} else {
					n[6] = 0;
				}
			} else {
				n[5] =  n[6] = 0;
			}
			if (gr[12].end - gr[12].start > 1) {
				if (std::string(str, gr[13].start - 1, 1).compare("+") == 0) {
					oph = "-" + std::string(str, gr[13].start, gr[13].end - gr[13].start);
					opm = "-" + std::string(str, gr[14].start, gr[14].end - gr[14].start);
				} else {
					oph = "+" + std::string(str, gr[13].start, gr[13].end - gr[13].start);
					opm = "+" + std::string(str, gr[14].start, gr[14].end - gr[14].start);
				}
				calculate_date(n, oph, "h");
				calculate_date(n, opm, "m");
			}
		} else {
			n[3] = n[4] = n[5] = n[6] = 0;
		}

		if (!validate_date(n)) {
			return "";
		}

		//Processing Date Math
		std::string date_math;
		len = gr[16].end - gr[16].start;
		if (len != 0) {
			date_math.assign(str, gr[16].start, len);
			if (gr) {
				free(gr);
				gr = NULL;
			}
			while (pcre_search(date_math.c_str(), len, offset, 0, DATE_MATH_RE, &compiled_date_math_re, &gr) == 0) {
				offset = gr[0].end;
				calculate_date(n, std::string(date_math, gr[1].start, gr[1].end - gr[1].start), std::string(date_math, gr[2].start, gr[2].end - gr[2].start));
			}
			if (gr) {
				free(gr);
				gr = NULL;
			}
			if (offset != len) {
				LOG(NULL, "ERROR: Date Math is used incorrectly.\n");
				return "";
			}
		}
		time_t tt = 0;
		struct tm *timeinfo = gmtime(&tt);
		timeinfo->tm_year   = n[0] - 1900;
		timeinfo->tm_mon    = n[1] - 1;
		timeinfo->tm_mday   = n[2];
		timeinfo->tm_hour   = n[3];
		timeinfo->tm_min    = n[4];
		timeinfo->tm_sec    = n[5];
		const time_t dateGMT = timegm(timeinfo);
		timestamp = (double) dateGMT;
		timestamp += n[6]/1000.0;
		return std::to_string(timestamp);
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	if (isNumeric(str)) {
		return str;
	}

	return "";
}


std::string get_prefix(const std::string &name, const std::string &prefix, char type)
{
	std::string slot = get_slot_hex(name);

	struct TRANSFORM {
		char operator() (char c) { return  c + 17;}
	};

	std::transform(slot.begin(), slot.end(), slot.begin(), TRANSFORM());
	std::string res(prefix);
	res.append(1, toupper(type));
	return res + slot;
}


std::string get_slot_hex(const std::string &name)
{
	std::string standard_name;
	if (strhasupper(name)) {
		standard_name = stringtoupper(name);
	} else {
		standard_name = name;
	}
	std::string _md5(md5(standard_name), 24, 8);
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


int get_coords(const std::string &str, double *coords)
{
	std::stringstream ss;
	group_t *g = NULL;
	int offset = 0, len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, offset, 0, COORDS_DISTANCE_RE, &compiled_coords_dist_re, &g);
	while (ret != -1 && (g[0].end - g[0].start) == len) {
		offset = g[0].end;
		/*LOG(NULL,"group[1] %s\n" , std::string(str.c_str() + g[1].start, g[1].end - g[1].start).c_str());
		 LOG(NULL,"group[2] %s\n" , std::string(str.c_str() + g[2].start, g[2].end - g[2].start).c_str());
		 LOG(NULL,"group[3] %s\n" , std::string(str.c_str() + g[3].start, g[3].end - g[3].start).c_str());*/
		ss.clear();
		ss << std::string(str.c_str() + g[1].start, g[1].end - g[1].start);
		ss >> coords[0];
		ss.clear();
		ss << std::string(str.c_str() + g[2].start, g[2].end - g[2].start);
		ss >> coords[1];
		ss.clear();
		ss << std::string(str.c_str() + g[3].start, g[3].end - g[3].start);
		ss >> coords[2];
		if (g[4].end - g[4].start > 0) {
			std::string units(str.c_str() + g[4].start, g[4].end - g[4].start);
			if (units.compare("mi") == 0) {
				coords[2] *= 1609.344;
			} else if (units.compare("km") == 0) {
				coords[2] *= 1000;
			} else if (units.compare("yd") == 0) {
				coords[2] *= 0.9144;
			} else if (units.compare("ft") == 0) {
				coords[2] *= 0.3048;
			} else if (units.compare("in") == 0) {
				coords[2] *= 0.0254;
			} else if (units.compare("cm") == 0) {
				coords[2] *= 0.01;
			} else if (units.compare("mm") == 0) {
				coords[2] *= 0.001;
			}
		}
		return 0;
	}
	return -1;
}


bool isRange(const std::string &str)
{
	group_t *gr = NULL;
	int ret = pcre_search(str.c_str(), (int)str.size(), 0, 0, FIND_RANGE_RE, &compiled_find_range_re , &gr);
	if (ret != -1) {
		if (gr) {
			free(gr);
			gr = NULL;
		}
		return true;
	}
	return false;
}


bool isLatLongDistance(const std::string &str)
{
	group_t *gr = NULL;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, COORDS_DISTANCE_RE, &compiled_coords_dist_re, &gr);
	if (ret != -1 && (gr[0].end - gr[0].start) == len) {
		if (gr) {
			free(gr);
			gr = NULL;
		}
		return true;
	}
	return false;
}


bool isNumeric(const std::string &str)
{
	group_t *g = NULL;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, NUMERIC_RE, &compiled_numeric_re, &g);
	if (ret != -1 && (g[0].end - g[0].start) == len) {
		if (g) {
			free(g);
			g = NULL;
		}
		return true;
	}
	return false;
}


bool StartsWith(const std::string &text, const std::string &token)
{
	if (text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}


void calculate_date(int n[], const std::string &op, const std::string &units)
{
	int num = strtoint(std::string(op.c_str() + 1, op.size())), max_days;
	time_t tt, dateGMT;
	struct tm *timeinfo;
	if (op.at(0) == '+' || op.at(0) == '-') {
		switch (units.at(0)) {
			case 'y':
				(op.at(0) == '+') ? n[0] += num : n[0] -= num; break;
			case 'M':
				if (op.at(0) == '+') {
					n[1] += num;
				} else {
					n[1] -= num;
				}
				tt = 0;
				timeinfo = gmtime(&tt);
				timeinfo->tm_year   = n[0] - 1900;
				timeinfo->tm_mon    = n[1] - 1;
				dateGMT = timegm(timeinfo);
				timeinfo = gmtime(&dateGMT);
				max_days = number_days(timeinfo->tm_year, n[1]);
				if (n[2] > max_days) n[2] = max_days;
				break;
			case 'w':
				(op.at(0) == '+') ? n[2] += 7 * num : n[2] -= 7 * num; break;
			case 'd':
				(op.at(0) == '+') ? n[2] += num : n[2] -= num; break;
			case 'h':
				(op.at(0) == '+') ? n[3] += num : n[3] -= num; break;
			case 'm':
				(op.at(0) == '+') ? n[4] += num : n[4] -= num; break;
			case 's':
				(op.at(0) == '+') ? n[5] += num : n[5] -= num; break;
		}
	} else {
		switch (units.at(0)) {
			case 'y':
				if (op.compare("/") == 0) {
					n[1] = 12;
					n[2] = number_days(n[0], 12);
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[1] = n[2] = 1;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'M':
				if (op.compare("/") == 0) {
					n[2] = number_days(n[0], n[1]);
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[2] = 1;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'w':
				tt = 0;
				timeinfo = gmtime(&tt);
				timeinfo->tm_year   = n[0] - 1900;
				timeinfo->tm_mon    = n[1] - 1;
				timeinfo->tm_mday   = n[2];
				dateGMT = timegm(timeinfo);
				timeinfo = gmtime(&dateGMT);
				if (op.compare("/") == 0) {
					n[2] += 6 - timeinfo->tm_wday;
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[2] -= timeinfo->tm_wday;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'd':
				if (op.compare("/") == 0) {
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'h':
				if (op.compare("/") == 0) {
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'm':
				if (op.compare("/") == 0) {
					n[5] = 59;
					n[6] = 999;
				} else {
					n[5] = n[6] = 0;
				}
				break;
			case 's':
				if (op.compare("/") == 0) {
					n[6] = 999;
				} else {
					n[6] = 0;
				}
			break;
		}
	}

	// Calculate new date
	tt = 0;
	timeinfo = gmtime(&tt);
	timeinfo->tm_year   = n[0] - 1900;
	timeinfo->tm_mon    = n[1] - 1;
	timeinfo->tm_mday   = n[2];
	timeinfo->tm_hour   = n[3];
	timeinfo->tm_min    = n[4];
	timeinfo->tm_sec    = n[5];
	dateGMT = timegm(timeinfo);
	timeinfo = gmtime(&dateGMT);
	n[0] = timeinfo->tm_year + 1900;
	n[1] = timeinfo->tm_mon + 1;
	n[2] = timeinfo->tm_mday;
	n[3] = timeinfo->tm_hour;
	n[4] = timeinfo->tm_min;
	n[5] = timeinfo->tm_sec;
}


bool validate_date(int n[])
{
	if (n[0] >= 1582) { //Gregorian calendar
		if (n[1] == 2 && !((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 28) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 28 days\n");
			return false;
		} else if(n[1] == 2 && ((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 29) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 29 days\n");
			return false;
		}
	} else {
		if (n[1] == 2 && n[0] % 4 != 0 && n[2] > 28) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 28 days\n");
			return false;
		} else if(n[1] == 2 && n[0] % 4 == 0 && n[2] > 29) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 29 days\n");
			return false;
		}
	}

	if ((n[1] == 4 || n[1] == 6 || n[1] == 9 || n[1] == 11) && n[2] > 30) {
		LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 30 days\n");
		return false;
	}

	return true;
}


int number_days(int year, int month)
{
	if (year >= 1582) { //Gregorian calendar
		if (month == 2 && !((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
			return 28;
		} else if(month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
			return 29;
		}
	} else {
		if (month == 2 && year % 4 != 0) {
			return 28;
		} else if(month == 2 && year % 4 == 0) {
			return 29;
		}
	}

	if(month == 4 || month == 6 || month == 9 || month == 11) {
		return 30;
	}

	return 31;
}


std::string
unserialise(char field_type, const std::string &field_name, const std::string &serialise_val)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return std::to_string(Xapian::sortable_unserialise(serialise_val));
		case DATE_TYPE:
			return unserialise_date(serialise_val);
		case GEO_TYPE:
			return unserialise_geo(serialise_val);
		case BOOLEAN_TYPE:
			return (serialise_val.at(0) == 'f') ? "false" : "true";
		case STRING_TYPE:
			return serialise_val;
	}
	return "";
}


std::string
serialise(char field_type, const std::string &field_value)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return serialise_numeric(field_value);
		case DATE_TYPE:
			return serialise_date(field_value);
		case GEO_TYPE:
			return serialise_geo(field_value);
		case BOOLEAN_TYPE:
			return serialise_bool(field_value);
		case STRING_TYPE:
			return field_value;
	}
	return "";
}


int identify_cmd(std::string &commad)
{
	if (!is_digits(commad)) {
		if (strcasecmp(commad.c_str(), "_search") == 0) {
			return CMD_SEARCH;
		} else if (strcasecmp(commad.c_str(), "_facets") == 0) {
			return CMD_FACETS;
		} else if (strcasecmp(commad.c_str(), "_stats") == 0) {
			return CMD_STATS;
		} else if (strcasecmp(commad.c_str(), "_schema") == 0) {
			return CMD_SCHEMA;
		}
		return CMD_UNKNOWN;
	} else return CMD_NUMBER;
}


bool is_digits(const std::string &str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit);
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


bool Is_id_range(std::string &ids)
{
	int len = (int) ids.size(), offset = 0;
	group_t *g = NULL;
	while ((pcre_search(ids.c_str(), len, offset, 0, RANGE_ID_RE, &compiled_range_id_re, &g)) != -1) {
		offset = g[0].end;
		if (g[1].end - g[1].start && g[2].end - g[2].start) {
			return true;
		} else {
			if(g[1].end - g[1].start){
				return true;
			} else {
				return false;
			}
		}
	}
	return false;
}


std::string to_type(std::string type)
{
	std::string low = stringtolower(type);
	if (low.compare("numeric") == 0 || low.compare("n") == 0) {
		return std::string("N");
	}else if (low.compare("geospatial") == 0 || low.compare("g") == 0) {
		return std::string("G");
	}else if (low.compare("string") == 0 || low.compare("s") == 0) {
		return std::string("S");
	}else if (low.compare("boolean") == 0 || low.compare("b") == 0) {
		return std::string("B");
	}else if (low.compare("date") == 0 || low.compare("d") == 0) {
		return std::string("D");
	} else {
		return std::string("S");
	}
}


void delete_files(std::string path)
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

	Subdir =readdir(Dir);
	while(Subdir) {
		if ( Subdir->d_type == isFolder) {
			if (strcmp(Subdir->d_name,".") != 0 && strcmp(Subdir->d_name,"..") != 0) {
				contains_folder = true;
			}
		}

		if ( Subdir->d_type == isFile) {
			std::string file = path + "/" + std::string(Subdir->d_name);
			if (remove(file.c_str()) != 0) {
				LOG_ERR(NULL,"file %s could not be deleted\n",Subdir->d_name);
			}
		}
		Subdir = readdir(Dir);
	}

	if (!contains_folder) {
		if (rmdir(path.c_str()) !=0 ) {
			LOG_ERR(NULL,"Directory %s could not be deleted\n",path.c_str());
		}
	}
}


void move_files(std::string src, std::string dst)
{
	unsigned char isFile = 0x8;

	DIR *Dir;
	struct dirent *Subdir;
	Dir = opendir(src.c_str());

	if (!Dir) {
		return;
	}

	Subdir =readdir(Dir);
	while (Subdir) {
		if ( Subdir->d_type == isFile) {
			std::string old_name = src + "/" + Subdir->d_name;
			std::string new_name = dst + "/" + Subdir->d_name;
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				LOG_ERR(NULL, "Couldn't rename %s to %s",old_name.c_str(), new_name.c_str());
			}
		}
		Subdir = readdir(Dir);
	}

	if (rmdir(src.c_str()) !=0 ) {
		LOG_ERR(NULL,"Directory %s could not be deleted\n",src.c_str());
	}
}