/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <memory>                  // for std::unique_ptr
#include <utility>                 // for std::move

#include <asio.hpp>

#include "reactor.h"               // for reactor::Reactor


namespace engine {

// Destroys object on the reactor's offload pool and resumes on the caller's loop, so a destructor
// that does database work never runs on a loop thread. With no pool it destroys inline.
template <typename T>
asio::awaitable<void> release_on_pool(reactor::Reactor* reactor, std::unique_ptr<T> object) {
	if (!object) {
		co_return;
	}
	if (reactor->pool() == nullptr) {
		object.reset();
		co_return;
	}
	co_await asio::co_spawn(reactor->pool()->get_executor(),
		[&object]() -> asio::awaitable<void> {
			object.reset();
			co_return;
		}, asio::use_awaitable);
}

}  // namespace engine
