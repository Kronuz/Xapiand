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

#include <xapian.h>

#include <iostream>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <string>


#define TRACEBACKS 1


std::string traceback(const char *filename, int line);


class Exception : public std::runtime_error {
protected:
	std::string msg;
	std::string context;
	std::string traceback;

public:
	Exception(const char *filename, int line, const char *format="", ...);
	~Exception() = default;

	const char* what() const noexcept override {
		return msg.c_str();
	}

	const char* get_context() const noexcept {
		return context.c_str();
	}

	const char* get_traceback() const noexcept {
		return traceback.c_str();
	}
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
	SerialisationError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::SerialisationError("") { }
};


class DatetimeError : public ClientError {
public:
	template<typename... Args>
	DatetimeError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class CartesianError : public ClientError {
public:
	template<typename... Args>
	CartesianError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class EWKTError : public ClientError {
public:
	template<typename... Args>
	EWKTError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


#define MSG_Error(...) Error(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_ClientError(...) ClientError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_LimitError(...) LimitError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_SerialisationError(...) SerialisationError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_DatetimeError(...) DatetimeError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_CartesianError(...) CartesianError(__FILE__, __LINE__, __VA_ARGS__)
#define MSG_EWKTError(...) EWKTError(__FILE__, __LINE__, __VA_ARGS__)
