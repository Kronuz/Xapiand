/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <utility>                                      // for std::forward

#include "engine/error.h"                                // for engine::Error, engine::errc
#include "xapiand_exception.h"                            // for ClientError


// Client errors that are also engine errors of one kind.
class QueryParserError : public ClientError, public engine::Error {
public:
	template<typename... Args>
	QueryParserError(Args&&... args) : ClientError(std::forward<Args>(args)...), engine::Error(engine::errc::syntax, message) { }
};


// A query nested deeper, or with more nodes, than engine::MAX_QUERY_DEPTH and MAX_QUERY_CLAUSES allow.
class QueryDepthError : public QueryParserError {
public:
	template<typename... Args>
	QueryDepthError(Args&&... args) : QueryParserError(std::forward<Args>(args)...) { }
};


// A range a terms-only field cannot answer exactly: the reason, without the field name, which the query
// builder adds.
class TermsOnlyRangeError : public QueryParserError {
public:
	template<typename... Args>
	TermsOnlyRangeError(Args&&... args) : QueryParserError(std::forward<Args>(args)...) { }
};


// Settings of a field's text that the parser cannot read.
class InvalidTextConfigError : public ClientError, public engine::Error {
public:
	template<typename... Args>
	InvalidTextConfigError(Args&&... args) : ClientError(std::forward<Args>(args)...), engine::Error(engine::errc::invalid, message) { }
};


class SerialisationError : public ClientError {
public:
	template<typename... Args>
	SerialisationError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class CastError : public ClientError {
public:
	template<typename... Args>
	CastError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};
