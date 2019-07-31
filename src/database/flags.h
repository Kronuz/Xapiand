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

const int DB_WRITABLE                   = 0x00010000;  // Opens as writable.
const int DB_RESTORE                    = 0x00020000;  // Flag database as being restored
const int DB_REPLICA                    = 0x00040000;  // Flag database as being replicated
const int DB_DISABLE_AUTOCOMMIT         = 0x00080000;  // Disable autocommit
const int DB_DISABLE_WAL                = 0x00100000;  // Disable open WAL file
const int DB_DISABLE_WRITES             = 0x00200000;  // Disable write operations
const int DB_SYNCHRONOUS_WAL            = 0x00400000;  // Using synchronous WAL
const int DB_TRIGGER_REPLICATION        = 0x00800000;  // Allow trigger replication

const int DB_RETRIES                    = 10;     // Number of tries to do an operation on a Xapian::Database or Document


inline bool has_db_create_or_open(int flags) {
	int action = flags & Xapian::DB_ACTION_MASK_;
	return action == DB_CREATE_OR_OPEN;
}

inline bool has_db_create_or_overwrite(int flags) {
	int action = flags & Xapian::DB_ACTION_MASK_;
	return action == DB_CREATE_OR_OVERWRITE;
}

inline bool has_db_create(int flags) {
	int action = flags & Xapian::DB_ACTION_MASK_;
	return action == DB_CREATE;
}

inline bool has_db_open(int flags) {
	int action = flags & Xapian::DB_ACTION_MASK_;
	return action == DB_OPEN;
}

inline bool has_db_writable(int flags) {
	return (flags & DB_WRITABLE) == DB_WRITABLE;
}

inline bool has_db_restore(int flags) {
	return (flags & DB_RESTORE) == DB_RESTORE;
}

inline bool has_db_replica(int flags) {
	return (flags & DB_REPLICA) == DB_REPLICA;
}

inline bool has_db_disable_autocommit(int flags) {
	return (flags & DB_DISABLE_AUTOCOMMIT) == DB_DISABLE_AUTOCOMMIT;
}

inline bool has_db_disable_wal(int flags) {
	return (flags & DB_DISABLE_WAL) == DB_DISABLE_WAL;
}

inline bool has_db_disable_writes(int flags) {
	return (flags & DB_DISABLE_WRITES) == DB_DISABLE_WRITES;
}

inline bool has_db_synchronous_wal(int flags) {
	return (flags & DB_SYNCHRONOUS_WAL) == DB_SYNCHRONOUS_WAL;
}

inline bool has_db_trigger_replication(int flags) {
	return (flags & DB_TRIGGER_REPLICATION) == DB_TRIGGER_REPLICATION;
}

inline std::string readable_flags(int flags) {
	std::vector<std::string> values;
	if (has_db_create_or_open(flags)) values.push_back("DB_CREATE_OR_OPEN");
	if (has_db_create_or_overwrite(flags)) values.push_back("DB_CREATE_OR_OVERWRITE");
	if (has_db_create(flags)) values.push_back("DB_CREATE");
	if (has_db_open(flags)) values.push_back("DB_OPEN");
	if (has_db_writable(flags)) values.push_back("DB_WRITABLE");
	if (has_db_restore(flags)) values.push_back("DB_RESTORE");
	if (has_db_replica(flags)) values.push_back("DB_REPLICA");
	if (has_db_disable_autocommit(flags)) values.push_back("DB_DISABLE_AUTOCOMMIT");
	if (has_db_disable_wal(flags)) values.push_back("DB_DISABLE_WAL");
	if (has_db_disable_writes(flags)) values.push_back("DB_DISABLE_WRITES");
	if (has_db_synchronous_wal(flags)) values.push_back("DB_SYNCHRONOUS_WAL");
	return strings::join(values, "|");
}
