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

#include "udp_base.h"

#include "server_discovery.h"

#define XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION 1
#define XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION 0

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;


// Discovery for nodes and databases
class Discovery : public BaseUDP {
private:
	ev::timer heartbeat;

	void heartbeat_cb(ev::timer &watcher, int revents);

	friend DiscoveryServer;

public:
	enum class Message {
		HELLO,         // New node saying hello
		WAVE,          // Nodes waving hello to the new node
		SNEER,         // Nodes telling the client they don't agree on the new node's name
		HEARTBEAT,     // Heartbeat
		BYE,           // Node says goodbye
		DB,            //
		DB_WAVE,       //
		BOSSY_DB_WAVE, //
		DB_UPDATED,    //
		MAX            //
	};

	Discovery(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string &group_);
	~Discovery();

	void send_message(Message type, const std::string &content);

	std::string getDescription() const noexcept override;

	std::function<void()> start = [this](){ heartbeat.again(); };
};
