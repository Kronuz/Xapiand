/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "udp_base.h"

// Values in seconds
#define HEARTBEAT_EXPLORE 0.100
#define HEARTBEAT_MIN 1.0
#define HEARTBEAT_MAX 2.0

#define XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION 1
#define XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION 0

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;


// Discovery for nodes and databases
class Discovery : public BaseUDP {
private:
	ev::timer heartbeat;
	ev::async async_enter;

	void heartbeat_cb(ev::timer& watcher, int revents);
	void async_enter_cb(ev::async &watcher, int revents);

	void _enter();

public:
	enum class Message {
		HEARTBEAT,     // Heartbeat
		HELLO,         // New node saying hello
		WAVE,          // Nodes waving hello to the new node
		SNEER,         // Nodes telling the client they don't agree on the new node's name
		ENTER,         // Node enters the room
		BYE,           // Node says goodbye
		DB,            //
		DB_WAVE,       //
		BOSSY_DB_WAVE, //
		DB_UPDATED,    //
		MAX,           //
	};

	static constexpr const char* const MessageNames[] = {
		"HEARTBEAT", "HELLO", "WAVE", "SNEER", "ENTER", "BYE", "DB", "DB_WAVE",
		"BOSSY_DB_WAVE", "DB_UPDATED",
	};

	Discovery(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& group_);
	~Discovery();

	inline void enter() {
		async_enter.send();
	}
	void start();
	void stop();

	inline void send_message(Message type, const std::string& message) {
		if (type != Discovery::Message::HEARTBEAT) {
			L_DISCOVERY(this, "<< send_message(%s)", MessageNames[toUType(type)]);
		}
		L_DISCOVERY_PROTO(this, "message: '%s'", repr(message).c_str());
		BaseUDP::send_message(toUType(type), message);
	}

	std::string getDescription() const noexcept override;
};

#endif
