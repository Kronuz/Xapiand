/*
* Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
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

#include <memory>           // for shared_ptr
#include <string>           // for string
#include <unordered_map>    // for unordered_map
#include <unordered_set>    // for unordered_set
#include <xapian.h>         // for Query, Query::op, termcount

#include "msgpack.h"        // for MsgPack
#include "schema.h"         // for Schema, FieldType, required_spc_t
#include "utils.h"


constexpr const char QUERYDSL_FROM[]   = "_from";
constexpr const char QUERYDSL_IN[]     = "_in";
constexpr const char QUERYDSL_QUERY[]  = "_query";
constexpr const char QUERYDSL_RANGE[]  = "_range";
constexpr const char QUERYDSL_RAW[]    = "_raw";
constexpr const char QUERYDSL_TO[]     = "_to";


/* A domain-specific language (DSL) for query */


class QueryDSL {
	std::shared_ptr<Schema> schema;

	using dispatch_func = Xapian::Query (QueryDSL::*)(const std::string&, Xapian::Query::op, const std::string&, const MsgPack&, Xapian::termcount, int, bool, bool, bool);

	static const std::unordered_map<std::string, dispatch_func> map_dispatch;

	FieldType get_in_type(const MsgPack& obj);

	std::pair<FieldType, MsgPack> parse_guess_range(const required_spc_t& field_spc, const std::string& range);
	MsgPack parse_range(const required_spc_t& field_spc, const std::string& range);


	/*
	 * Dispatch functions.
	 */

	Xapian::Query process_in(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_range(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_raw(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_maybe(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_not(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_elite_set(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_filter(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_max(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_near(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_or(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_phrase(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_scale_weight(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_synonym(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_ge(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_le(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_range(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_wildcard(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_xor(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_cast(const std::string& word, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);


	Xapian::Query process(Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query get_value_query(const std::string& path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);

	Xapian::Query get_acc_date_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_time_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_timedelta_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_num_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_geo_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_accuracy_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf, bool is_in);
	Xapian::Query get_namespace_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_term_query(const required_spc_t& field_spc, std::string& serialised_term, Xapian::termcount wqf, int q_flags, bool is_wildcard);
	Xapian::Query get_in_query(const required_spc_t& field_spc, const MsgPack& obj);

public:
	QueryDSL(const std::shared_ptr<Schema>& schema_);

	MsgPack make_dsl_query(const std::string& query);
	MsgPack make_dsl_query(const query_field_t& e);

	Xapian::Query get_query(const MsgPack& obj);
};
