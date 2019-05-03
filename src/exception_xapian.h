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

#include "exception.h"                            // for ClientError
#include "xapian.h"                               // for DocNotFoundError, InternalError, InvalidArgument, ...


class DocVersionConflictError : public ClientError, public Xapian::DocVersionConflictError {
public:
	template<typename... Args>
	DocVersionConflictError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::DocVersionConflictError(message) { }
};


class SerialisationError : public ClientError, public Xapian::SerialisationError {
public:
	template<typename... Args>
	SerialisationError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::SerialisationError(message) { }
};


class CastError : public ClientError, public Xapian::SerialisationError {
public:
	template<typename... Args>
	CastError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::SerialisationError(message) { }
};


class NetworkError : public ClientError, public Xapian::NetworkError {
public:
	template<typename... Args>
	NetworkError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::NetworkError(message) { }
};


class InvalidArgumentError : public ClientError, public Xapian::InvalidArgumentError {
public:
	template<typename... Args>
	InvalidArgumentError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::InvalidArgumentError(message) { }
};


class InvalidOperationError : public ClientError, public Xapian::InvalidOperationError {
public:
	template<typename... Args>
	InvalidOperationError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::InvalidOperationError(message) { }
};


class QueryParserError : public ClientError, public Xapian::QueryParserError {
public:
	template<typename... Args>
	QueryParserError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::QueryParserError(message) { }
};


class InternalError : public ClientError, public Xapian::InternalError {
public:
	template<typename... Args>
	InternalError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::InternalError(message) { }
};
