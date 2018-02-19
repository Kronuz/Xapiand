/*
* Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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
#include "string_view.h"    // for string_view


constexpr const char QUERYDSL_FROM[]   = "_from";
constexpr const char QUERYDSL_IN[]     = "_in";
constexpr const char QUERYDSL_QUERY[]  = "_query";
constexpr const char QUERYDSL_RANGE[]  = "_range";
constexpr const char QUERYDSL_RAW[]    = "_raw";
constexpr const char QUERYDSL_TO[]     = "_to";


/* A domain-specific language (DSL) for query */


class QueryDSL {
	std::shared_ptr<Schema> schema;

	using dispatch_func = Xapian::Query (QueryDSL::*)(string_view, Xapian::Query::op, string_view, const MsgPack&, Xapian::termcount, int, bool, bool, bool);

	static const std::unordered_map<string_view, dispatch_func> map_dispatch;

	FieldType get_in_type(const MsgPack& obj);

	std::pair<FieldType, MsgPack> parse_guess_range(const required_spc_t& field_spc, string_view range);
	MsgPack parse_range(const required_spc_t& field_spc, string_view range);


	/*
	 * Dispatch functions.
	 */

	Xapian::Query process_in(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_range(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_raw(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_maybe(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_not(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_elite_set(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_filter(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_max(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_near(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_or(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_phrase(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_scale_weight(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_synonym(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_ge(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_le(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_range(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_wildcard(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_xor(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_cast(string_view word, Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);


	Xapian::Query process(Xapian::Query::op op, string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query get_value_query(string_view path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);

	Xapian::Query get_acc_date_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_time_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_timedelta_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_num_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_geo_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_accuracy_query(const required_spc_t& field_spc, string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf, bool is_in);
	Xapian::Query get_namespace_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_term_query(const required_spc_t& field_spc, string_view serialised_term, Xapian::termcount wqf, int q_flags, bool is_wildcard);
	Xapian::Query get_in_query(const required_spc_t& field_spc, const MsgPack& obj);

public:
	QueryDSL(const std::shared_ptr<Schema>& schema_);

	MsgPack make_dsl_query(string_view query);
	MsgPack make_dsl_query(const query_field_t& e);

	Xapian::Query get_query(const MsgPack& obj);
};
