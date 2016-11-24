/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include <xapian.h>     // for DocNotFoundError, InternalError, InvalidArgum...
#include <stdexcept>    // for runtime_error
#include <string>       // for string
#include <type_traits>  // for forward


#ifdef XAPIAND_TRACEBACKS
#define TRACEBACK (traceback(__FILE__, __LINE__).c_str())
#endif


std::string traceback(const char *filename, int line);


class BaseException {
protected:
	std::string message;
	std::string context;
	std::string traceback;

public:
	BaseException(const char *filename, int line, const char *format, ...);
	BaseException(const char *filename, int line, const std::string& message="")
		: BaseException(filename, line, message.c_str()) { }

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
	DummyException() : BaseException(__FILE__, __LINE__, "Dummy Exception"), std::runtime_error(message) { }
};

class CheckoutError : public Exception {
public:
	template<typename... Args>
	CheckoutError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
};

class Exit : public BaseException, public std::runtime_error {
public:
	int code;
	Exit(int code_) : BaseException(__FILE__, __LINE__, "Exit"), std::runtime_error(message), code(code_) { }
};


class Error : public Exception {
public:
	template<typename... Args>
	Error(Args&&... args) : Exception(std::forward<Args>(args)...) { }
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


class QueryDslError : public ClientError {
public:
	template<typename... Args>
	QueryDslError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};

#define THROW(exc, ...) throw exc(__FILE__, __LINE__, ##__VA_ARGS__)

#define MSG_Error(...) Error(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_ClientError(...) ClientError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_LimitError(...) LimitError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_SerialisationError(...) SerialisationError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_DummyException() DummyException()
#define MSG_NetworkError(...) NetworkError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_InvalidArgumentError(...) InvalidArgumentError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_InvalidOperationError(...) InvalidOperationError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_QueryParserError(...) QueryParserError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_InternalError(...) InternalError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_DocNotFoundError(...) DocNotFoundError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_CheckoutError(...) CheckoutError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_MissingTypeError(...) MissingTypeError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_QueryDslError(...) QueryDslError(__FILE__, __LINE__, __VA_ARGS__)
