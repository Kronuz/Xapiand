/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <stdexcept>          // for runtime_error
#include <string>             // for string
#include "string_view.hh"     // for std::string_view
#include <type_traits>        // for forward
#include <xapian.h>           // for DocNotFoundError, InternalError, InvalidArgum...

#include "fmt/printf.h"       // fmt::printf_args, fmt::vsprintf, fmt::make_printf_args


#define TRACEBACK() ::traceback(__func__, __FILE__, __LINE__)

std::string traceback(const char* function, const char* filename, int line, int skip = 1);

class BaseException {
	static const BaseException& default_exc() {
		static const BaseException default_exc;
		return default_exc;
	}

	BaseException(const BaseException& exc, const char *function_, const char *filename_, int line_, const char* type, std::string_view format, fmt::printf_args args);

protected:
	std::string type;
	const char* function;
	const char* filename;
	int line;
	void* callstack[128];
	size_t frames;

	mutable std::string message;
	mutable std::string context;
	mutable std::string traceback;

	BaseException();

public:
	BaseException(const BaseException& exc);
	BaseException(BaseException&& exc);

	BaseException(const BaseException* exc);

	template <typename... Args>
	BaseException(const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(default_exc(), function, filename, line, type, format, fmt::make_printf_args(std::forward<Args>(args)...)) { }
	template <typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
	BaseException(const T* exc, const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(*exc, function, filename, line, type, format, fmt::make_printf_args(std::forward<Args>(args)...)) { }
	template <typename... Args>
	BaseException(const void*, const char *function, const char *filename, int line, const char* type, std::string_view format, Args&&... args)
		: BaseException(default_exc(), function, filename, line, type, format, fmt::make_printf_args(std::forward<Args>(args)...)) { }

	BaseException(const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(default_exc(), function, filename, line, type, msg, fmt::make_printf_args()) { }
	template <typename T, typename = std::enable_if_t<std::is_base_of<BaseException, std::decay_t<T>>::value>>
	BaseException(const T* exc, const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(*exc, function, filename, line, type, msg, fmt::make_printf_args()) { }
	BaseException(const void*, const char *function, const char *filename, int line, const char* type, std::string_view msg = "")
		: BaseException(default_exc(), function, filename, line, type, msg, fmt::make_printf_args()) { }

	virtual ~BaseException() = default;

	const char* get_message() const;
	const char* get_context() const;
	const char* get_traceback() const;

	bool empty() const {
		return type.empty();
	}
};


class Exception : public BaseException, public std::runtime_error {
public:
	template<typename... Args>
	Exception(Args&&... args) : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
};


class CheckoutError : public Exception {
public:
	template<typename... Args>
	CheckoutError(Args&&... args) : Exception(std::forward<Args>(args)...) { }
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


class CheckoutErrorEndpointNotAvailable : public CheckoutError {
public:
	template<typename... Args>
	CheckoutErrorEndpointNotAvailable(Args&&... args) : CheckoutError(std::forward<Args>(args)...) { }
};


class Exit : public BaseException, public std::runtime_error {
public:
	int code;
	explicit Exit(int code_) : BaseException(__func__, __FILE__, __LINE__, "Exit"), std::runtime_error(message), code(code_) { }
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


class NotFoundError : public ClientError {
public:
	template<typename... Args>
	NotFoundError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class DatabaseNotFoundError : public NotFoundError, public Xapian::DatabaseOpeningError {
public:
	template<typename... Args>
	DatabaseNotFoundError(Args&&... args) : NotFoundError(std::forward<Args>(args)...), Xapian::DatabaseOpeningError(message) { }
};


class DocNotFoundError : public NotFoundError, public Xapian::DocNotFoundError {
public:
	template<typename... Args>
	DocNotFoundError(Args&&... args) : NotFoundError(std::forward<Args>(args)...), Xapian::DocNotFoundError(message) { }
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
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define THROW(exception, ...) throw exception(__func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)
#define RETHROW(exception, ...) throw exception(&exc, __func__, __FILE__, __LINE__, #exception, ##__VA_ARGS__)

#pragma GCC diagnostic pop
