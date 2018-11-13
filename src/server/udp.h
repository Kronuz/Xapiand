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
#include <memory>        // for shared_ptr
#include <string>        // for string

#include "worker.h"      // for Worker


constexpr int UDP_SO_REUSEPORT     = 1;


class UDP {
protected:
	struct sockaddr_in addr;

	int port;
	int sock;
	std::atomic_bool closed;

	int flags;

	const char* description;

	uint8_t major_version;
	uint8_t minor_version;

	void bind(int tries, const std::string& group);
	void find(int tries, const std::string& group);

	bool close();

	void sending_message(const std::string& message);

public:
	UDP(int port, const char* description, uint8_t major_version, uint8_t minor_version, int flags);
	~UDP();

	void send_message(char type, const std::string& content);
	char get_message(std::string& result, char max_type);
};
