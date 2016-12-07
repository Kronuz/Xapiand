/*
* Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include <xapian.h>         // for Query, Query::op, termcount
#include <memory>           // for shared_ptr
#include <string>           // for string
#include <unordered_map>    // for unordered_map
#include <unordered_set>    // for unordered_set

#include "msgpack.h"        // for MsgPack
#include "schema.h"         // for Schema, FieldType, required_spc_t
#include "utils.h"


constexpr const char QUERYDSL_QUERY[] = "_query";

/* A domain-specific language (DSL) for query */

class QueryDSL {
	std::shared_ptr<Schema> schema;

	static const std::unordered_map<std::string, Xapian::Query::op> ops_map;
	static const std::unordered_set<std::string> casts_set;

	Xapian::Query process(Xapian::Query::op op, const std::string& parent, const MsgPack& obj);
	Xapian::Query process_in(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj);
	Xapian::Query get_value_query(Xapian::Query::op op, const std::string& path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool isrange=false);
	Xapian::Query get_accuracy_query(const required_spc_t& field_spc, Xapian::Query::op op, const std::string& field_accuracy, const MsgPack& obj, bool isrange);
	Xapian::Query get_namespace_query(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool isrange);
	Xapian::Query get_regular_query(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool isrange);

public:
	QueryDSL(std::shared_ptr<Schema> schema_);

	MsgPack make_dsl_query(const std::string& query);
	MsgPack make_dsl_query(const query_field_t& e);

	Xapian::Query get_query(const MsgPack& obj);
};
