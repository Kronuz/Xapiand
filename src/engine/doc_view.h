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

#include <functional>              // for std::function
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "engine/types.h"          // for docid, termcount, valueno


namespace engine {

// Called once per term of a document.
using TermSink = std::function<void(std::string_view term, termcount wdf)>;

// A stored document read lazily from a shard: the engine fetches only what is asked for.
class DocView {
public:
	virtual ~DocView() = default;

	virtual docid did() const = 0;
	virtual std::string data() const = 0;
	virtual std::string value(valueno slot) const = 0;
	virtual rev version() const = 0;
	virtual void terms(std::string_view prefix, const TermSink& sink) const = 0;
};

}  // namespace engine
