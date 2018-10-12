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

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "udp_base.h"


// Values in seconds
constexpr double WAITING_FAST        = 0.2;
constexpr double WAITING_SLOW        = 1.0;
constexpr double HEARTBEAT_MIN       = 2.0;
constexpr double HEARTBEAT_MAX       = 4.0;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;

class DiscoveryServer;

// Discovery for nodes and databases
class Discovery : public BaseUDP {
	friend DiscoveryServer;

private:
	ev::timer heartbeat;
	ev::async enter_async;

	void heartbeat_cb(ev::timer& watcher, int revents);
	void enter_async_cb(ev::async& watcher, int revents);

	void _enter();

public:
	std::string __repr__() const override {
		return Worker::__repr__("Discovery");
	}

	enum class Message {
		HEARTBEAT,     // Heartbeat
		HELLO,         // New node saying hello
		WAVE,          // Nodes waving hello to the new node
		SNEER,         // Nodes telling the client they don't agree on the new node's name
		ENTER,         // Node enters the room
		BYE,           // Node says goodbye
		DB_UPDATED,    //
		MAX,           //
	};

	static const std::string& MessageNames(Message type) {
		static const std::string MessageNames[] = {
			"HEARTBEAT", "HELLO", "WAVE", "SNEER", "ENTER", "BYE", "DB_UPDATED",
		};

		auto type_int = static_cast<int>(type);
		if (type_int >= 0 || type_int < static_cast<int>(Message::MAX)) {
			return MessageNames[type_int];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

	Discovery(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_);
	~Discovery();

	inline void enter() {
		enter_async.send();
	}

	void start();
	void stop();

	void send_message(Message type, const std::string& message);
	std::string getDescription() const noexcept override;
};

#endif
