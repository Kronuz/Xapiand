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

#include "config.h"             // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

#include "base_udp.h"
#include "database.h"


// Values in seconds
constexpr double WAITING_FAST  = 0.200;
constexpr double WAITING_SLOW  = 0.600;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;


// Discovery for nodes and databases
class Discovery : public UDP, public Worker {
public:
	enum class Message {
		HELLO,         // New node saying hello
		WAVE,          // Nodes telling the client they do agree with the new node's name
		SNEER,         // Nodes telling the client they don't agree with the new node's name
		ENTER,         // Node enters the room
		BYE,           // Node says goodbye
		DB_UPDATED,    //
		MAX,           //
	};

	static const std::string& MessageNames(Message type) {
		static const std::string MessageNames[] = {
			"HELLO", "WAVE", "SNEER", "ENTER", "BYE", "DB_UPDATED",
		};

		auto type_int = static_cast<int>(type);
		if (type_int >= 0 || type_int < static_cast<int>(Message::MAX)) {
			return MessageNames[type_int];
		}
		static const std::string UNKNOWN = "UNKNOWN";
		return UNKNOWN;
	}

private:
	ev::io io;
	ev::timer discovery;

	void send_message(Message type, const std::string& message);
	void io_accept_cb(ev::io& watcher, int revents);
	void discovery_server(Discovery::Message type, const std::string& message);

	void hello(Message type, const std::string& message);
	void wave(Message type, const std::string& message);
	void sneer(Message type, const std::string& message);
	void enter(Message type, const std::string& message);
	void bye(Message type, const std::string& message);
	void db_updated(Message type, const std::string& message);

	void discovery_cb(ev::timer& watcher, int revents);

	void destroyer();

	void destroy_impl() override;
	void shutdown_impl(time_t asap, time_t now) override;

	// No copy constructor
	Discovery(const Discovery&) = delete;
	Discovery& operator=(const Discovery&) = delete;

public:
	Discovery(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_);
	~Discovery();

	void start();
	void stop();

	void signal_db_update(const DatabaseUpdate& update);

	std::string __repr__() const override {
		return Worker::__repr__("Discovery");
	}

	std::string getDescription() const noexcept override;
};

#endif
