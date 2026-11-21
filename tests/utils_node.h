/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// A node that a test runs in its own process: the port it serves on and a connection to its listener.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace test_utils {

// A TCP port the OS has just handed out, kept bound while the reservation lives, so no other process's bind(0)
// takes it. Where the node binds with SO_REUSEPORT (the condition in XapiandManager::make_servers) its listener
// binds beside the reservation; elsewhere the node cannot, so the port is released at once.
class ReservedPort {
public:
	ReservedPort() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
		const int on = 1;
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		socklen_t len = sizeof(addr);
		if (fd_ < 0 || ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0
#if defined(__linux__) || defined(SO_REUSEPORT_LB)
			|| ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) != 0
#endif
			|| ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
			std::fprintf(stderr, "FAIL: no free port\n");
			std::exit(1);
		}
		port_ = ntohs(addr.sin_port);
#if !(defined(__linux__) || defined(SO_REUSEPORT_LB))
		release();
#endif
	}

	~ReservedPort() { release(); }

	ReservedPort(const ReservedPort&) = delete;
	ReservedPort& operator=(const ReservedPort&) = delete;

	int port() const { return port_; }

private:
	void release() {
		if (fd_ >= 0) {
			::close(fd_);
			fd_ = -1;
		}
	}

	int fd_;
	int port_ = 0;
};

// A connection to the node's listener, -1 when there is none by `limit`: the node is READY a moment before its
// listener accepts, so a refused connect is tried again.
inline int connect_to(int port, std::chrono::milliseconds limit = std::chrono::seconds(5)) {
	const auto end = std::chrono::steady_clock::now() + limit;
	while (true) {
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (fd >= 0 && ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
			return fd;
		}
		if (fd >= 0) {
			::close(fd);
		}
		if (std::chrono::steady_clock::now() >= end) {
			return -1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

}  // namespace test_utils
