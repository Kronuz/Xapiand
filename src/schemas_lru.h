/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include <mutex>

#include "atomic_shared_ptr.h"
#include "endpoint.h"
#include "lru.h"
#include "msgpack.h"
#include "schema.h"


constexpr size_t MAX_SCHEMA_RECURSION = 10;


class DatabaseHandler;


class SchemasLRU : public lru::LRU<size_t, atomic_shared_ptr<const MsgPack>> {
	void validate_metadata(DatabaseHandler* db_handler, const std::shared_ptr<const MsgPack>& local_schema_ptr, std::string& schema_path, std::string& schema_id);
	std::shared_ptr<const MsgPack> validate_string_meta_schema(MsgPack new_schema, const MsgPack& schema_value, const std::array<FieldType, SPC_SIZE_TYPES>& sep_types, std::string& schema_path, std::string& schema_id);
	std::shared_ptr<const MsgPack> validate_object_meta_schema(MsgPack new_schema, const MsgPack& schema_value, const std::array<FieldType, SPC_SIZE_TYPES>& sep_types);
	std::shared_ptr<const MsgPack> validate_meta_schema(MsgPack new_schema, const MsgPack& schema_value, const std::array<FieldType, SPC_SIZE_TYPES>& sep_types, std::string& schema_path, std::string& schema_id);
	bool get_strict(const MsgPack& obj, bool flag_strict);
	std::tuple<bool, atomic_shared_ptr<const MsgPack>*, std::string, std::string> get_local(DatabaseHandler* db_handler, const MsgPack* obj=nullptr);
	MsgPack get_shared(const Endpoint& endpoint, const std::string& id, std::shared_ptr<std::unordered_set<size_t>> context);

	std::mutex smtx;

public:
	SchemasLRU(ssize_t max_size=-1)
		: LRU(max_size) { }

	std::shared_ptr<const MsgPack> get(DatabaseHandler* db_handler, const MsgPack* obj);
	bool set(DatabaseHandler* db_handler, std::shared_ptr<const MsgPack>& old_schema, const std::shared_ptr<const MsgPack>& new_schema);
};
