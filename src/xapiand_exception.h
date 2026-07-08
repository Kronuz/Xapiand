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

// The exception base (BaseException, Exception, Error, InvalidArgument,
// OutOfRange, SystemExit) and the THROW/RETHROW macros now live in the standalone
// located-exception library (github.com/Kronuz/located-exception), fetched via
// CMake. This wrapper is named xapiand_exception.h (not exception.h) so it never
// shadows the library's exception.h on the include path -- extracted deps compiled
// into Xapiand (msgpack, compressors) do a bare #include "exception.h" for the base
// and must resolve it to located-exception's, not to this wrapper (which adds
// Xapiand-specific subclasses). Reaching the base is now a plain include off the
// path (no absolute-path macro).
#include "exception.h"

#include <type_traits>       // for std::forward

// Xapiand-specific exception subclasses, dropped from the standalone library to
// keep it generic. They are part of Xapiand's schema/query error vocabulary.

class ClientError : public Exception {
public:
	template<typename... Args>
	ClientError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


class MissingTypeError : public ClientError {
public:
	template<typename... Args>
	MissingTypeError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class QueryDslError : public ClientError {
public:
	template<typename... Args>
	QueryDslError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};
