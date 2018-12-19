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

#include <mutex>                 // for std::mutex
#include "string_view.hh"        // for std::string_view

#include "atomic_shared_ptr.h"
#include "endpoint.h"
#include "lru.h"
#include "msgpack.h"
#include "schema.h"


constexpr size_t MAX_SCHEMA_RECURSION = 10;


class DatabaseHandler;


class SchemasLRU : lru::LRU<std::string, atomic_shared_ptr<const MsgPack>> {
	template <typename ErrorType>
	std::pair<const MsgPack*, const MsgPack*> validate_schema(const MsgPack& object, const char* prefix, std::string_view& foreign, std::string_view& foreign_path, std::string_view& foreign_id);

	MsgPack get_shared(const Endpoint& endpoint, std::string_view id, std::shared_ptr<std::unordered_set<std::string>> context);

	std::mutex smtx;

public:
	SchemasLRU(ssize_t max_size=-1)
		: LRU(max_size) { }

	std::tuple<std::shared_ptr<const MsgPack>, std::unique_ptr<MsgPack>, std::string> get(DatabaseHandler* db_handler, const MsgPack* obj, bool write);
	bool set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema);
	bool drop(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema);
};
