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

#pragma once

#include "../manager.h"

// Values in seconds
#define HEARTBEAT_MIN 0.250
#define HEARTBEAT_MAX 0.500

#define XAPIAND_UDP_PROTOCOL_MAJOR_VERSION 1
#define XAPIAND_UDP_PROTOCOL_MINOR_VERSION 0

const uint16_t XAPIAND_UDP_PROTOCOL_VERSION = XAPIAND_UDP_PROTOCOL_MAJOR_VERSION | XAPIAND_UDP_PROTOCOL_MINOR_VERSION << 8;

class XapiandManager;


// Base class for sending UDP messages
class BaseUDP {
private:
	XapiandManager *manager;
	ev::loop_ref *loop;

	struct sockaddr_in addr;
	int port;
	int sock;

	std::string group;

	void send_message(const char *buf, size_t buf_size);

public:
	enum class Message;

	BaseUDP(XapiandManager *manager, ev::loop_ref *loop_, int port_, const std::string &group_, const std::string &description);
	virtual ~Raft();

	void send_message(const Message type, const std::string &content) const;
};
