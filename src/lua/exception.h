/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <stdexcept>    // for runtime_error
#include <string>       // for string

#include "xapiand_exception.h"  // for ClientError


namespace lua {

class Error : public std::runtime_error {
	using std::runtime_error::runtime_error;
};


// A script crossed one of its bounds (instructions, memory, matcher steps, callbacks): a 400.
class ScriptLimitError : public ClientError {
public:
	template<typename... Args>
	ScriptLimitError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


// A script raised an error of its own: a 400, and the document is as it was.
class ScriptError : public ClientError {
public:
	template<typename... Args>
	ScriptError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class TimeOutError : public Error {
public:
	TimeOutError() : Error("Time Out") { }
};


class ScriptNotFoundError : public Error {
public:
	explicit ScriptNotFoundError(const std::string& what_arg) : Error(what_arg) { }
};


class ScriptSyntaxError : public Error {
public:
	explicit ScriptSyntaxError(const std::string& what_arg) : Error(what_arg) { }
};


class InvalidArgument : public Error {
public:
	explicit InvalidArgument(const std::string& what_arg) : Error(what_arg) { }
};

}; // End namespace lua
