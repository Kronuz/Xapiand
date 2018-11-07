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

#include <atomic>        // for std::atomic_bool
#include <netinet/in.h>  // for sockaddr_in
#include <sys/types.h>   // for uint16_t
#include <time.h>        // for time_t
#include <memory>        // for shared_ptr
#include <string>        // for string

#include "worker.h"      // for Worker


class UDP {
protected:
	struct sockaddr_in addr;

	int port;
	int sock;
	std::atomic_bool closed;

	std::string description;
	uint8_t major_version;
	uint8_t minor_version;

	void sending_message(const std::string& message);

	void close();

public:
	UDP(int port_, std::string  description_, uint8_t major_, uint8_t minor_, const std::string& group_, int tries_=1);
	virtual ~UDP();

	void send_message(char type, const std::string& content);
	char get_message(std::string& result, char max_type);

	void bind(int tries, const std::string& group);

	virtual std::string getDescription() const noexcept = 0;
};


// Base class for UDP messages configuration
class BaseUDP : public UDP, public Worker {
protected:
	void shutdown_impl(time_t asap, time_t now) override;
	void destroy_impl() override;

public:
	BaseUDP(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, std::string  description_, uint8_t major_version_, uint8_t minor_version_, const std::string& group_, int tries_=1);
	~BaseUDP();
};
