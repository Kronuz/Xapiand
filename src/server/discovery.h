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

#include "debouncer.h"          // for make_debouncer
#include "thread.hh"            // for ThreadPolicyType::*
#include "udp.h"                // for UDP


// Values in seconds
constexpr double WAITING_FAST  = 0.200;
constexpr double WAITING_SLOW  = 0.600;

constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION = 0;

struct DatabaseUpdate;
class UUID;

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
		static const std::string _[] = {
			"HELLO", "WAVE", "SNEER", "ENTER", "BYE", "DB_UPDATED",
		};
		auto type_int = static_cast<size_t>(type);
		if (type_int >= 0 || type_int < sizeof(_) / sizeof(_[0])) {
			return _[type_int];
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

	void shutdown_impl(long long asap, long long now) override;
	void destroy_impl() override;
	void start_impl() override;
	void stop_impl() override;

	// No copy constructor
	Discovery(const Discovery&) = delete;
	Discovery& operator=(const Discovery&) = delete;

public:
	Discovery(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port, const std::string& group);
	~Discovery();

	std::string __repr__() const override {
		return Worker::__repr__("Discovery");
	}

	void db_update_send(const std::string& path);

	std::string getDescription() const;
};

void db_updater_send(std::string path);

inline auto& db_updater() {
	static auto db_updater = make_debouncer<std::string, 3000, 6000, 12000, ThreadPolicyType::updaters>("U--", "U%02zu", 3, db_updater_send);
	return db_updater;
}

#endif
