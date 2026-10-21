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

#include <string>                  // for std::string

#include "engine/error.h"          // for Error, errc
#include "xapian.h"                // for the Xapian exception types


namespace engine::xapian {

// An exception that is both an engine error of one kind and the Xapian exception it stands for, so
// callers can catch either until every caller has moved to the kinds.
template <errc Kind, typename X>
class Failure : public Error, public X {
public:
	explicit Failure(const std::string& reason) : Error(Kind, reason), X(reason) { }

	// Keeps everything the original carries (system error, context, type name).
	explicit Failure(const X& original) : Error(Kind, original.get_msg()), X(original) { }
};

using NotFound = Failure<errc::not_found, Xapian::DatabaseNotFoundError>;
using NotAvailable = Failure<errc::not_available, Xapian::DatabaseNotAvailableError>;
using Reopen = Failure<errc::reopen, Xapian::DatabaseModifiedError>;
using Corrupt = Failure<errc::corrupt, Xapian::DatabaseCorruptError>;
using Opening = Failure<errc::opening, Xapian::DatabaseOpeningError>;
using Closed = Failure<errc::closed, Xapian::DatabaseClosedError>;
using Timeout = Failure<errc::timeout, Xapian::NetworkTimeoutError>;
using Network = Failure<errc::network, Xapian::NetworkError>;
using DocNotFound = Failure<errc::doc_not_found, Xapian::DocNotFoundError>;
using VersionConflict = Failure<errc::version_conflict, Xapian::DocVersionConflictError>;
using DatabaseFailure = Failure<errc::database, Xapian::DatabaseError>;
using InvalidArgument = Failure<errc::invalid, Xapian::InvalidArgumentError>;
using InvalidOperation = Failure<errc::invalid, Xapian::InvalidOperationError>;

// Rethrows the Xapian exception being handled as the Failure of its kind. An exception that is
// already an engine error, or that is not a Xapian database failure, is rethrown unchanged.
// Call only inside a catch block.
[[noreturn]] void rethrow_translated();

}  // namespace engine::xapian
