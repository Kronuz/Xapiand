/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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

#include "xapiand.h"

#include <stdexcept>    // for runtime_error
#include <string>       // for string
#include <type_traits>  // for forward
#include <xapian.h>     // for DocNotFoundError, InternalError, InvalidArgum...


#define TRACEBACK() traceback(__FILE__, __LINE__)

std::string traceback(const char *filename, int line);


class BaseException {
protected:
	std::string message;
	std::string context;
	std::string traceback;

public:
	BaseException(const char *filename, int line, const char* type, const char* format, ...);

	BaseException(const char *filename, int line, const char* type, const std::string& msg="")
		: BaseException(filename, line, type, msg.c_str()) { }

	virtual ~BaseException() = default;

	virtual const char* get_message() const noexcept {
		return message.c_str();
	}

	virtual const char* get_context() const noexcept {
		return context.c_str();
	}

	virtual const char* get_traceback() const noexcept {
		return traceback.c_str();
	}
};


class Exception : public BaseException, public std::runtime_error {
public:
	template<typename... Args>
	Exception(Args&&... args) : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
};


class DummyException : public BaseException, public std::runtime_error {
public:
	DummyException() : BaseException(__FILE__, __LINE__, "DummyException"), std::runtime_error(message) { }
};


class CheckoutError : public Exception {
public:
	template<typename... Args>
	CheckoutError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


class CheckoutErrorCommited : public CheckoutError {
public:
	template<typename... Args>
	CheckoutErrorCommited(Args&&... args) : CheckoutError(std::forward<Args>(args)...) { }
};


class CheckoutErrorReplicating : public CheckoutError {
public:
	template<typename... Args>
	CheckoutErrorReplicating(Args&&... args) : CheckoutError(std::forward<Args>(args)...) { }
};


class CheckoutErrorBadEndpoint : public CheckoutError {
public:
	template<typename... Args>
	CheckoutErrorBadEndpoint(Args&&... args) : CheckoutError(std::forward<Args>(args)...) { }
};


class Exit : public BaseException, public std::runtime_error {
public:
	int code;
	explicit Exit(int code_) : BaseException(__FILE__, __LINE__, "Exit"), std::runtime_error(message), code(code_) { }
};


class Error : public Exception {
public:
	template<typename... Args>
	Error(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


class TimeOutError : public Exception {
public:
	template<typename... Args>
	TimeOutError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


class ClientError : public Exception {
public:
	template<typename... Args>
	ClientError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};


class LimitError : public Exception {
public:
	template<typename... Args>
	LimitError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
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


class DocNotFoundError : public ClientError, public Xapian::DocNotFoundError {
public:
	template<typename... Args>
	DocNotFoundError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::DocNotFoundError(message) { }
};


class MissingTypeError : public ClientError {
public:
	template<typename... Args>
	MissingTypeError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class ForeignSchemaError : public MissingTypeError {
public:
	template<typename... Args>
	ForeignSchemaError(Args&&... args) : MissingTypeError(std::forward<Args>(args)...) { }
};


class QueryDslError : public ClientError {
public:
	template<typename... Args>
	QueryDslError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


// Wrapped standard exceptions:

class InvalidArgument : public BaseException, public std::invalid_argument {
public:
	template<typename... Args>
	InvalidArgument(Args&&... args) : BaseException(std::forward<Args>(args)...), std::invalid_argument(message) { }
};


class OutOfRange : public BaseException, public std::out_of_range {
public:
	template<typename... Args>
	OutOfRange(Args&&... args) : BaseException(std::forward<Args>(args)...), std::out_of_range(message) { }
};


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define THROW(exc, ...) throw exc(__FILE__, __LINE__, #exc, ##__VA_ARGS__)

#pragma GCC diagnostic pop
