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

#include <memory>                // for std::shared_ptr
#include <mutex>                 // for std::mutex
#include <string_view>           // for std::string_view

#include "database/schema.h"
#include "endpoint.h"
#include "lru.h"
#include "msgpack.h"
#include "xapian.h"              // for Xapian::rev


constexpr size_t MAX_SCHEMA_RECURSION = 10;


class DatabaseHandler;


class SchemasLRU {
	mutable std::mutex schemas_mtx;
	lru::aging_lru<std::string, std::shared_ptr<const MsgPack>> schemas;

	mutable std::mutex versions_mtx;
	lru::aging_lru<std::string, Xapian::rev> versions;

	std::tuple<bool, std::shared_ptr<const MsgPack>, std::string, std::string>
	_update(
		const char* prefix,
		bool writable,
		const std::shared_ptr<const MsgPack>& new_schema, const MsgPack* schema_obj,
		const Endpoints& endpoints, int read_flags, std::shared_ptr<std::unordered_set<std::string>> context);

public:
	SchemasLRU(ssize_t max_size, std::chrono::milliseconds max_age);

	std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string> get(DatabaseHandler* db_handler, const MsgPack* obj);

	bool set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema);

	void updated(const std::string& uri, Xapian::rev version);

	void cleanup();

	std::string __repr__() const;

	std::string dump_schemas(int level = 1) const;
};
