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

// A Host over an Asio executor: the control loop's tasks (a timer's commit, an idle close) run here.

#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <utility>                 // for std::move

#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include "engine/host.h"           // for Host, HostScope


namespace engine {

class AsioHost final : public Host {
public:
	explicit AsioHost(asio::any_io_executor executor) : executor_(std::move(executor)) { }

	void post(std::coroutine_handle<> h) override {
		asio::post(executor_, [this, h] {
			HostScope host(this);
			task::running_on scope(task::scheduler_ref::of(*this));
			h.resume();
		});
	}

protected:
	std::unique_ptr<Timer> arm(std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> node) override {
		auto state = std::make_shared<State>(executor_, at, std::move(node));
		state->timer.async_wait([state](const asio::error_code& ec) {
			if (!ec && !state->cancelled) {
				state->node->fire();
			}
		});
		return std::make_unique<Armed>(std::move(state));
	}

private:
	struct State {
		State(const asio::any_io_executor& ex, std::chrono::steady_clock::time_point at, std::unique_ptr<TimerNode> n) : timer(ex, at), node(std::move(n)) { }
		asio::steady_timer timer;
		std::unique_ptr<TimerNode> node;
		bool cancelled = false;  // the loop's thread only: a timer is armed and cancelled there
	};

	class Armed final : public Timer {
	public:
		explicit Armed(std::shared_ptr<State> state) : state_(std::move(state)) { }
		~Armed() override {
			state_->cancelled = true;
			state_->timer.cancel();
		}

	private:
		std::shared_ptr<State> state_;
	};

	asio::any_io_executor executor_;
};

}  // namespace engine
