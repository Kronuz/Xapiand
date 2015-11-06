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

#include "udp_base.h"

#include <assert.h>


BaseUDP::BaseUDP(XapiandManager *manager_, ev::loop_ref *loop_, int port_, const std::string &group_, const std::string &description_)
	: manager(manager_),
	  loop(loop_),
	  port(port_),
	  group(group_)
{
	sock = bind_udp(description.c_str(), port, addr, 1, group.c_str());

	LOG(this, "Listening sock=%d\n", sock);
	assert(sock != -1);
}


BaseUDP::~BaseUDP() { }


void BaseUDP::send_message(const char *buf, size_t buf_size)
{
	std::unique_lock<std::mutex> lk(manager()->get_lock());
	if (sock != -1) {
		LOG_UDP_WIRE(this, "(sock=%d) <<-- '%s'\n", sock, repr(buf, buf_size).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(sock, buf, buf_size, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr));
#else
		ssize_t written = ::sendto(sock, buf, buf_size, 0, (struct sockaddr *)&addr, sizeof(addr));
#endif

		if (written < 0) {
			if (sock != -1 && !ignored_errorno(errno, true)) {
				LOG_ERR(this, "ERROR: sendto error (sock=%d): %s\n", sock, strerror(errno));
				manager()->destroy();
			}
		}
	}
}


void BaseUDP::send_message(const Message type, const std::string &content) const
{
	if (!content.empty()) {
		std::string message((const char *)&type, 1);
		message.append(std::string((const char *)&XAPIAND_UDP_PROTOCOL_VERSION, sizeof(uint16_t)));
		message.append(serialise_string(manager->cluster_name));
		message.append(content);
		send_message(message.c_str(), message.size());
	}
}
