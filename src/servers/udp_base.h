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
#define HEARTBEAT_MIN 1 // 0.250
#define HEARTBEAT_MAX 2 // 0.500


// Base class for UDP messages configuration
class BaseUDP : public Worker {
protected:
	struct sockaddr_in addr;

	int port;
	int sock;

	std::string description;

	void sending_message(const std::string& message);

public:
	BaseUDP(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_, int port_, const std::string& description_, const std::string& group_, int tries_=1);
	~BaseUDP();

	void shutdown(bool asap, bool now);
	void destroy();

	virtual std::string getDescription() const noexcept = 0;

	void bind(int tries, const std::string& group);

	inline decltype(auto) manager() noexcept {
		return share_parent<XapiandManager>();
	}
};
