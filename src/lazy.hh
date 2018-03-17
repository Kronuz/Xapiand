/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <utility>
#include <ostream>

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
	friend std::ostream& operator<<(std::ostream& os, const lazy_eval<L> & obj) {
		return os << obj();
	}
};

template <typename L>
lazy_eval<L>
make_lazy_eval(L&& lambda) {
	return {std::forward<L>(lambda)};
}

#define LAZY(Expr) make_lazy_eval([&]() -> decltype((Expr)) { return (Expr); })
