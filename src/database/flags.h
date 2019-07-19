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

#include "strings.hh"         // for strings::join
#include "xapian.h"           // for Xapian::*

const int DB_CREATE_OR_OPEN             = Xapian::DB_CREATE_OR_OPEN;            // Create database if it doesn't already exist.
const int DB_CREATE_OR_OVERWRITE        = Xapian::DB_CREATE_OR_OVERWRITE;       // Create database if it doesn't already exist, or overwrite if it does.
const int DB_CREATE                     = Xapian::DB_CREATE;                    // Create a new database.
const int DB_OPEN                       = Xapian::DB_OPEN;                      // Open an existing database.

const int DB_WRITABLE                   = 0x1000;  // Opens as writable.
const int DB_RESTORE                    = 0x2000;  // Flag database as being restored
const int DB_REPLICA                    = 0x3000;  // Flag database as being replicated
const int DB_DISABLE_WAL                = 0x4000;  // Disable open WAL file
const int DB_SYNCHRONOUS_WAL            = 0x5000;  // Using synchronous WAL

const int DB_RETRIES                    = 10;     // Number of tries to do an operation on a Xapian::Database or Document


inline std::string readable_flags(int flags) {
	std::vector<std::string> values;
	if ((flags & DB_CREATE_OR_OPEN) == DB_CREATE_OR_OPEN) values.push_back("DB_CREATE_OR_OPEN");
	if ((flags & DB_CREATE_OR_OVERWRITE) == DB_CREATE_OR_OVERWRITE) values.push_back("DB_CREATE_OR_OVERWRITE");
	if ((flags & DB_CREATE) == DB_CREATE) values.push_back("DB_CREATE");
	if ((flags & DB_OPEN) == DB_OPEN) values.push_back("DB_OPEN");
	if ((flags & DB_WRITABLE) == DB_WRITABLE) values.push_back("DB_WRITABLE");
	if ((flags & DB_RESTORE) == DB_RESTORE) values.push_back("DB_RESTORE");
	if ((flags & DB_REPLICA) == DB_REPLICA) values.push_back("DB_REPLICA");
	if ((flags & DB_DISABLE_WAL) == DB_DISABLE_WAL) values.push_back("DB_DISABLE_WAL");
	if ((flags & DB_SYNCHRONOUS_WAL) == DB_SYNCHRONOUS_WAL) values.push_back("DB_SYNCHRONOUS_WAL");
	return strings::join(values, "|");
}
