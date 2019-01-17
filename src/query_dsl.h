/*
* Copyright (C) 2015-2019 Dubalu LLC. All rights reserved.
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

#include <memory>                 // for shared_ptr
#include <string>                 // for string
#include "string_view.hh"         // for std::string_view
#include <unordered_map>          // for unordered_map
#include <unordered_set>          // for unordered_set
#include <xapian.h>               // for Query, Query::op, termcount

#include "msgpack.h"              // for MsgPack
#include "schema.h"               // for Schema, FieldType, required_spc_t
#include "multivalue/keymaker.h"  // for Multi_MultiValueKeyMaker"


constexpr const char QUERYDSL_FROM[]            = "_from";
constexpr const char QUERYDSL_IN[]              = "_in";
constexpr const char QUERYDSL_QUERY[]           = "_query";
constexpr const char QUERYDSL_RANGE[]           = "_range";
constexpr const char QUERYDSL_RAW[]             = "_raw";
constexpr const char QUERYDSL_TO[]              = "_to";
constexpr const char QUERYDSL_LIMIT[]           = "_limit";
constexpr const char QUERYDSL_CHECK_AT_LEAST[]  = "_check_at_least";
constexpr const char QUERYDSL_OFFSET[]          = "_offset";
constexpr const char QUERYDSL_SORT[]            = "_sort";
constexpr const char QUERYDSL_SELECTOR[]        = "_selector";
constexpr const char QUERYDSL_ORDER[]           = "_order";
constexpr const char QUERYDSL_METRIC[]          = "_metric";

constexpr const char QUERYDSL_ASC[]             = "asc";
constexpr const char QUERYDSL_DESC[]            = "desc";

/* A domain-specific language (DSL) for query */


class QueryDSL {
	std::shared_ptr<Schema> schema;

	FieldType get_in_type(const MsgPack& obj);

	std::pair<FieldType, MsgPack> parse_guess_range(const required_spc_t& field_spc, std::string_view range);
	MsgPack parse_range(const required_spc_t& field_spc, std::string_view range);


	/*
	 * Dispatch functions.
	 */

	Xapian::Query process_in(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_range(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_raw(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_maybe(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_and_not(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_elite_set(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_filter(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_max(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_near(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_or(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_phrase(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_scale_weight(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_synonym(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_ge(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_le(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_value_range(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_wildcard(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_xor(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query process_cast(std::string_view word, Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);


	Xapian::Query process(Xapian::Query::op op, std::string_view parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);
	Xapian::Query get_value_query(std::string_view path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard);

	Xapian::Query get_acc_date_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_time_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_timedelta_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_num_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_acc_geo_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf);
	Xapian::Query get_accuracy_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf, bool is_in);
	Xapian::Query get_namespace_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard);
	Xapian::Query get_term_query(const required_spc_t& field_spc, std::string_view serialised_term, Xapian::termcount wqf, int q_flags, bool is_wildcard);
	Xapian::Query get_in_query(const required_spc_t& field_spc, const MsgPack& obj);

	void create_2exp_op_dsl(std::vector<MsgPack>& stack_msgpack, const std::string& operator_dsl);
	void create_exp_op_dsl(std::vector<MsgPack>& stack_msgpack, const std::string& operator_dsl);

public:
	QueryDSL(std::shared_ptr<Schema>  schema_);

	MsgPack make_dsl_query(std::string_view query);
	MsgPack make_dsl_query(const query_field_t& e);

	Xapian::Query get_query(const MsgPack& obj);
	void get_sorter(std::unique_ptr<Multi_MultiValueKeyMaker>& sorter, const MsgPack& obj);
};
