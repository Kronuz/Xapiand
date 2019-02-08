/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <ostream>           // for std::ostream
#include <type_traits>       // for std::enable_if_t
#include <utility>           // for std::forward, std::move


template <typename F, std::enable_if_t<std::is_invocable<F&>::value, int> = 0>
auto eval(F&& invocable) {
	return invocable();
}


template <typename Val, std::enable_if_t<not std::is_invocable<Val&>::value, int> = 0>
auto eval(Val&& val) {
	return std::forward<Val>(val);
}


template <class L>
class lazy_eval {
	const L& lambda;

public:
	lazy_eval(const L& lambda) : lambda(lambda) {}
	lazy_eval(lazy_eval&& other) : lambda(std::move(other.lambda)) { }
	lazy_eval& operator=(lazy_eval&& other) {
		lambda = std::move(other.lambda);
	}

	lazy_eval(const lazy_eval&) = delete;
	lazy_eval& operator=(const lazy_eval&) = delete;

	using expression_type = decltype(std::declval<L>()());

	explicit operator expression_type() const { return lambda(); }

	expression_type operator()() const { return lambda(); }

	friend std::ostream& operator<<(std::ostream& os, const lazy_eval<L>& obj) {
		return os << obj();
	}
};


template <typename L>
lazy_eval<L>
make_lazy_eval(L&& lambda) {
	return {std::forward<L>(lambda)};
}

#define LAZY(Expr) make_lazy_eval([&]() { return eval(Expr); })
