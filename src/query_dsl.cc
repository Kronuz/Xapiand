/*
* Copyright (C) 2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "query_dsl.h"

#include "booleanParser/BooleanParser.h"       // for BooleanTree
#include "booleanParser/LexicalException.h"    // for LexicalException
#include "booleanParser/SyntacticException.h"  // for SyntacticException
#include "cast.h"                              // for Cast
#include "database_utils.h"                    // for prefixed, RESERVED_VALUE
#include "exception.h"                         // for THROW, QueryDslError
#include "field_parser.h"                      // for FieldParser
#include "log.h"                               // for L_CALL, L
#include "multivalue/generate_terms.h"         // for GenerateTerms
#include "multivalue/geospatialrange.h"        // for GeoSpatial, GeoSpatialRange
#include "multivalue/range.h"                  // for MultipleValueRange
#include "serialise.h"                         // for MsgPack, get_range_type...
#include "utils.h"                             // for repr, startswith


#ifndef L_QUERY
#define L_QUERY_DEFINED
#define L_QUERY(args...)
#endif


const std::unordered_map<std::string, QueryDSL::dispatch_func> QueryDSL::map_dispatch({
	// Leaf query clauses.
	{ QUERYDSL_IN,                    &QueryDSL::process_in            },
	{ QUERYDSL_RANGE,                 &QueryDSL::process_range         },
	{ QUERYDSL_RAW,                   &QueryDSL::process_raw           },
	{ RESERVED_VALUE,                 &QueryDSL::process_value         },
	// Compound query clauses
	{ "_and",                         &QueryDSL::process_and           },
	{ "_and_maybe",                   &QueryDSL::process_and_maybe     },
	{ "_and_not",                     &QueryDSL::process_and_not       },
	{ "_elite_set",                   &QueryDSL::process_elite_set     },
	{ "_filter",                      &QueryDSL::process_filter        },
	{ "_max",                         &QueryDSL::process_max           },
	{ "_near",                        &QueryDSL::process_near          },
	{ "_not",                         &QueryDSL::process_and_not       },
	{ "_or",                          &QueryDSL::process_or            },
	{ "_phrase",                      &QueryDSL::process_phrase        },
	{ "_scale_weight",                &QueryDSL::process_scale_weight  },
	{ "_synonym",                     &QueryDSL::process_synonym       },
	{ "_value_ge",                    &QueryDSL::process_value_ge      },
	{ "_value_le",                    &QueryDSL::process_value_le      },
	{ "_value_range",                 &QueryDSL::process_value_range   },
	{ "_wildcard",                    &QueryDSL::process_wildcard      },
	{ "_xor",                         &QueryDSL::process_xor           },
	// Reserved cast words
	{ RESERVED_FLOAT,                 &QueryDSL::process_cast          },
	{ RESERVED_POSITIVE,              &QueryDSL::process_cast          },
	{ RESERVED_INTEGER,               &QueryDSL::process_cast          },
	{ RESERVED_BOOLEAN,               &QueryDSL::process_cast          },
	{ RESERVED_TERM,                  &QueryDSL::process_cast          },
	{ RESERVED_TEXT,                  &QueryDSL::process_cast          },
	{ RESERVED_DATE,                  &QueryDSL::process_cast          },
	{ RESERVED_UUID,                  &QueryDSL::process_cast          },
	{ RESERVED_EWKT,                  &QueryDSL::process_cast          },
	{ RESERVED_POINT,                 &QueryDSL::process_cast          },
	{ RESERVED_POLYGON,               &QueryDSL::process_cast          },
	{ RESERVED_CIRCLE,                &QueryDSL::process_cast          },
	{ RESERVED_CHULL,                 &QueryDSL::process_cast          },
	{ RESERVED_MULTIPOINT,            &QueryDSL::process_cast          },
	{ RESERVED_MULTIPOLYGON,          &QueryDSL::process_cast          },
	{ RESERVED_MULTICIRCLE,           &QueryDSL::process_cast          },
	{ RESERVED_MULTICONVEX,           &QueryDSL::process_cast          },
	{ RESERVED_MULTICHULL,            &QueryDSL::process_cast          },
	{ RESERVED_GEO_COLLECTION,        &QueryDSL::process_cast          },
	{ RESERVED_GEO_INTERSECTION,      &QueryDSL::process_cast          },
});


/* A domain-specific language (DSL) for query */


QueryDSL::QueryDSL(const std::shared_ptr<Schema>& schema_)
	: schema(schema_) { }


FieldType
QueryDSL::get_in_type(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_in_type(%s)", repr(obj.to_string()).c_str());

	try {
		auto it = obj.find(QUERYDSL_RANGE);
		if (it == obj.end()) {
			// If is not _range must be geo.
			return FieldType::GEO;
		}

		const auto& range = it.value();
		try {
			auto it_f = range.find(QUERYDSL_FROM);
			if (it_f == range.end()) {
				auto it_t = range.find(QUERYDSL_TO);
				if (it_t == range.end()) {
					return FieldType::EMPTY;
				} else {
					return Serialise::guess_type(it_t.value());
				}
			} else {
				return Serialise::guess_type(it_f.value());
			}
		} catch (const msgpack::type_error&) {
			THROW(QueryDslError, "%s must be object [%s]", QUERYDSL_RANGE, repr(range.to_string()).c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(QueryDslError, "%s must be object [%s]", QUERYDSL_IN, repr(obj.to_string()).c_str());
	}

	return FieldType::EMPTY;
}


std::pair<FieldType, MsgPack>
QueryDSL::parse_guess_range(const required_spc_t& field_spc, const std::string& range)
{
	L_CALL(this, "QueryDSL::parse_guess_range(<field_spc>, %s)", repr(range).c_str());

	FieldParser fp(range);
	fp.parse();
	if (!fp.is_range()) {
		THROW(QueryDslError, "Invalid range [<string>]: %s", repr(range).c_str());
	}
	MsgPack value;
	auto& _range = value[QUERYDSL_RANGE] = MsgPack(MsgPack::Type::MAP);
	auto start = fp.get_start();
	auto field_type = FieldType::EMPTY;
	if (!start.empty()) {
		auto& obj = _range[QUERYDSL_FROM] = Cast::cast(field_spc.get_type(), start);
		field_type = Serialise::guess_type(obj);
	}
	auto end = fp.get_end();
	if (!end.empty()) {
		auto& obj = _range[QUERYDSL_TO] = Cast::cast(field_spc.get_type(), end);
		if (field_type == FieldType::EMPTY) {
			field_type = Serialise::guess_type(obj);
		}
	}

	return std::make_pair(field_type, std::move(value));
}


MsgPack
QueryDSL::parse_range(const required_spc_t& field_spc, const std::string& range)
{
	L_CALL(this, "QueryDSL::parse_range(<field_spc>, %s)", repr(range).c_str());

	FieldParser fp(range);
	fp.parse();
	if (!fp.is_range()) {
		THROW(QueryDslError, "Invalid range [<string>]: %s", repr(range).c_str());
	}
	MsgPack value;
	auto& _range = value[QUERYDSL_RANGE] = MsgPack(MsgPack::Type::MAP);
	auto start = fp.get_start();
	if (!start.empty()) {
		_range[QUERYDSL_FROM] = Cast::cast(field_spc.get_type(), start);
	}
	auto end = fp.get_end();
	if (!end.empty()) {
		_range[QUERYDSL_TO] = Cast::cast(field_spc.get_type(), end);
	}

	return value;
}


Xapian::Query
QueryDSL::process_in(const std::string&, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_in(...)");

	return process(op, parent, obj, wqf, q_flags, is_raw, true, is_wildcard);
}


Xapian::Query
QueryDSL::process_range(const std::string& word, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_range(...)");

	return get_value_query(parent, {{ word, obj }}, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_raw(const std::string&, Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_raw(...)");

	return process(op, parent, obj, wqf, q_flags, true, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_value(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_value(...)");

	return get_value_query(parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_and(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_and(...)");

	return process(Xapian::Query::OP_AND, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_and_maybe(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_and_maybe(...)");

	return process(Xapian::Query::OP_AND_MAYBE, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_and_not(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_and_not(...)");

	return process(Xapian::Query::OP_AND_NOT, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_elite_set(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_elite_set(...)");

	return process(Xapian::Query::OP_ELITE_SET, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_filter(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_filter(...)");

	return process(Xapian::Query::OP_FILTER, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_max(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_max(...)");

	return process(Xapian::Query::OP_MAX, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_near(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_near(...)");

	return process(Xapian::Query::OP_NEAR, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_or(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_or(...)");

	return process(Xapian::Query::OP_OR, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_phrase(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_phrase(...)");

	return process(Xapian::Query::OP_PHRASE, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_scale_weight(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_scale_weight(...)");

	return process(Xapian::Query::OP_SCALE_WEIGHT, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_synonym(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_synonym(...)");

	return process(Xapian::Query::OP_SYNONYM, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_value_ge(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_value_ge(...)");

	return process(Xapian::Query::OP_VALUE_GE, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_value_le(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_value_le(...)");

	return process(Xapian::Query::OP_VALUE_LE, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_value_range(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_value_range(...)");

	return process(Xapian::Query::OP_VALUE_RANGE, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_wildcard(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool)
{
	L_CALL(this, "QueryDSL::process_wildcard(...)");

	return process(Xapian::Query::OP_WILDCARD, parent, obj, wqf, q_flags, is_raw, is_in, true);
}


Xapian::Query
QueryDSL::process_xor(const std::string&, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_xor(...)");

	return process(Xapian::Query::OP_XOR, parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process_cast(const std::string& word, Xapian::Query::op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process_cast(%s, ...)", repr(word).c_str());

	return get_value_query(parent, {{ word, obj }}, wqf, q_flags, is_raw, is_in, is_wildcard);
}


Xapian::Query
QueryDSL::process(Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::process(%d, %s, %s, <wqf>, <q_flags>, %s, %s, %s)", (int)op, repr(parent).c_str(), repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false", is_wildcard ? "true" : "false");

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}

	switch (obj.getType()) {
		case MsgPack::Type::MAP: {
			const auto it_e = obj.end();
			for (auto it = obj.begin(); it != it_e; ++it) {
				const auto field_name = it->as_string();
				auto const& o = it.value();

				L_QUERY(this, BLUE "%s = %s" NO_COL, field_name.c_str(), o.to_string().c_str());

				static const auto it_de = map_dispatch.end();

				Xapian::Query query;
				auto it_d = map_dispatch.find(field_name);
				if (it_d == it_de) {
					if (parent.empty()) {
						query = process(op, field_name, o, wqf, q_flags, is_raw, is_in, is_wildcard);
					} else {
						std::string n_parent;
						n_parent.reserve(parent.length() + 1 + field_name.length());
						n_parent.append(parent).append(1, DB_OFFSPRING_UNION).append(field_name);
						query = process(op, n_parent, o, wqf, q_flags, is_raw, is_in, is_wildcard);
					}
				} else {
					query = (this->*it_d->second)(field_name, op, parent, o, wqf, q_flags, is_raw, is_in, is_wildcard);
				}
				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;
		}

		case MsgPack::Type::ARRAY:
			for (auto const& o : obj) {
				auto query = process(op, parent, o, wqf, q_flags, is_raw, is_in, is_wildcard);
				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;

		default: {
			auto query = get_value_query(parent, obj, wqf, q_flags, is_raw, is_in, is_wildcard);
			final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			break;
		}
	}

	return final_query;
}


Xapian::Query
QueryDSL::get_value_query(const std::string& path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::get_value_query(%s, %s, <wqf>, <q_flags>, %s, %s, %s)", repr(path).c_str(), repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false", is_wildcard ? "true" : "false");

	if (path.empty()) {
		if (!is_in && is_raw && obj.is_string()) {
			const auto aux = Cast::cast(FieldType::EMPTY, obj.as_string());
			return get_namespace_query(default_spc, aux, wqf, q_flags, is_in, is_wildcard);
		}
		return get_namespace_query(default_spc, obj, wqf, q_flags, is_in, is_wildcard);
	} else {
		auto data_field = schema->get_data_field(path, is_in);
		const auto& field_spc = data_field.first;

		if (!data_field.second.empty()) {
			return get_accuracy_query(field_spc, data_field.second, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, wqf, is_in);
		}

		if (field_spc.flags.inside_namespace) {
			return get_namespace_query(field_spc, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, wqf, q_flags, is_in, is_wildcard);
		}

		try {
			return get_regular_query(field_spc, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, wqf, q_flags, is_in, is_wildcard);
		} catch (const SerialisationError&) {
			return get_namespace_query(field_spc, (!is_in && is_raw && obj.is_string()) ? Cast::cast(FieldType::EMPTY, obj.as_string()) : obj, wqf, q_flags, is_in, is_wildcard);
		}
	}
}


Xapian::Query
QueryDSL::get_acc_date_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL(this, "QueryDSL::get_acc_date_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy).c_str(), repr(obj.to_string()).c_str());

	auto it = map_acc_date.find(field_accuracy.substr(1));
	if (it != map_acc_date.end()) {
		Datetime::tm_t tm = Datetime::to_tm_t(obj);
		switch (it->second) {
			case UnitTime::SECOND: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::MINUTE: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::HOUR: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::DAY: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::MONTH: {
				Datetime::tm_t _tm(tm.year, tm.mon);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::YEAR: {
				Datetime::tm_t _tm(tm.year);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::DECADE: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::CENTURY: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
			case UnitTime::MILLENNIUM: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, required_spc_t::get_ctype(FieldType::DATE)), wqf);
			}
		}
	}

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
}


Xapian::Query
QueryDSL::get_acc_num_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL(this, "QueryDSL::get_acc_num_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy).c_str(), repr(obj.to_string()).c_str());

	try {
		auto acc = stox(std::stoull, field_accuracy.substr(1));
		auto value = Cast::integer(obj);
		return Xapian::Query(prefixed(Serialise::integer(value - modulus(value, acc)), field_spc.prefix, required_spc_t::get_ctype(FieldType::INTEGER)), wqf);
	} catch (const InvalidArgument&) {
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
	} catch (const OutOfRange&) {
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
	}
}


Xapian::Query
QueryDSL::get_acc_geo_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL(this, "QueryDSL::get_acc_geo_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy).c_str(), repr(obj.to_string()).c_str());

	if (field_accuracy.find("_geo") == 0) {
		try {
			auto nivel = stox(std::stoull, field_accuracy.substr(4));
			GeoSpatial geo(obj);
			const auto ranges = geo.getGeometry()->getRanges(default_spc.flags.partials, default_spc.error);
			return GenerateTerms::geo(ranges, { nivel }, { field_spc.prefix }, wqf);
		} catch (const InvalidArgument&) {
			THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
		} catch (const OutOfRange&) {
			THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
		}
	}

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
}


Xapian::Query
QueryDSL::get_accuracy_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj, Xapian::termcount wqf, bool is_in)
{
	L_CALL(this, "QueryDSL::get_accuracy_query(<field_spc>, %s, %s, <wqf>, %s)", repr(field_accuracy).c_str(), repr(obj.to_string()).c_str(), is_in ? "true" : "false");

	if (is_in) {
		THROW(QueryDslError, "Accuracy is only indexed like terms, searching by range is not supported");
	}

	switch (field_spc.get_type()) {
		case FieldType::INTEGER:
			return get_acc_num_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::DATE:
			return get_acc_date_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::GEO:
			return get_acc_geo_query(field_spc, field_accuracy, obj, wqf);
		default:
			THROW(Error, "Type: %s does not handle accuracy terms", Serialise::type(field_spc.get_type()).c_str());
	}
}


Xapian::Query
QueryDSL::get_namespace_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::get_namespace_query(<field_spc>, %s, <wqf>, <q_flags>, %s, %s)", repr(obj.to_string()).c_str(), is_in ? "true" : "false", is_wildcard ? "true" : "false");

	if (is_in) {
		if (obj.is_string()) {
			auto parsed = parse_guess_range(field_spc, obj.as_string());
			if (parsed.first == FieldType::EMPTY) {
				return Xapian::Query::MatchAll;
			} else if (field_spc.prefix.empty()) {
				return get_in_query(specification_t::get_global(parsed.first), parsed.second);
			} else {
				return get_in_query(Schema::get_namespace_specification(parsed.first, field_spc.prefix), parsed.second);
			}
		} else {
			auto field_type = get_in_type(obj);
			if (field_type == FieldType::EMPTY) {
				return Xapian::Query::MatchAll;
			} else if (field_spc.prefix.empty()) {
				return get_in_query(specification_t::get_global(field_type), obj);
			} else {
				return get_in_query(Schema::get_namespace_specification(field_type, field_spc.prefix), obj);
			}
		}
	}

	switch (obj.getType()) {
		case MsgPack::Type::NIL:
			return Xapian::Query(field_spc.prefix);
		case MsgPack::Type::STR: {
			auto val = obj.as_string();
			if (val.empty()) {
				return Xapian::Query(field_spc.prefix);
			} else if (val == "*") {
				return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix);
			}
			break;
		}
		default:
			break;
	}

	auto ser_type = Serialise::guess_serialise(obj);
	auto spc = Schema::get_namespace_specification(std::get<0>(ser_type), field_spc.prefix);

	return get_term_query(spc, std::get<1>(ser_type), wqf, q_flags, is_wildcard);
}


Xapian::Query
QueryDSL::get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_in, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::get_regular_query(<field_spc>, %s, <wqf>, <q_flags>, %s, %s)", repr(obj.to_string()).c_str(), is_in ? "true" : "false", is_wildcard ? "true" : "false");

	if (is_in) {
		if (obj.is_string()) {
			return get_in_query(field_spc, parse_range(field_spc, obj.as_string()));
		} else {
			return get_in_query(field_spc, obj);
		}
	}

	switch (obj.getType()) {
		case MsgPack::Type::NIL:
			return Xapian::Query(field_spc.prefix);
		case MsgPack::Type::STR: {
			auto val = obj.as_string();
			if (val.empty()) {
				return Xapian::Query(field_spc.prefix);
			} else if (val == "*") {
				return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix);
			}
			break;
		}
		default:
			break;
	}

	auto serialised_term = Serialise::MsgPack(field_spc, obj);
	return get_term_query(field_spc, serialised_term, wqf, q_flags, is_wildcard);
}


Xapian::Query
QueryDSL::get_term_query(const required_spc_t& field_spc, std::string& serialised_term, Xapian::termcount wqf, int q_flags, bool is_wildcard)
{
	L_CALL(this, "QueryDSL::get_term_query(<field_spc>, %s, <wqf>, <q_flags>, %s)", repr(serialised_term).c_str(), is_wildcard ? "true" : "false");

	switch (field_spc.get_type()) {
		case FieldType::TEXT: {
			Xapian::QueryParser parser;
			if (field_spc.flags.bool_term) {
				parser.add_boolean_prefix("_", field_spc.prefix + field_spc.get_ctype());
			} else {
				parser.add_prefix("_", field_spc.prefix + field_spc.get_ctype());
			}
			const auto& stopper = getStopper(field_spc.language);
			parser.set_stopper(stopper.get());
			parser.set_stemming_strategy(getQueryParserStemStrategy(field_spc.stem_strategy));
			parser.set_stemmer(Xapian::Stem(field_spc.stem_language));
			return parser.parse_query("_:" + serialised_term, q_flags);
		}

		case FieldType::STRING: {
			Xapian::QueryParser parser;
			if (field_spc.flags.bool_term) {
				parser.add_boolean_prefix("_", field_spc.prefix + field_spc.get_ctype());
			} else {
				parser.add_prefix("_", field_spc.prefix + field_spc.get_ctype());
			}
			return parser.parse_query("_:" + serialised_term, q_flags);
		}

		case FieldType::TERM: {
			if (!field_spc.flags.bool_term) {
				to_lower(serialised_term);
			}
			if (endswith(serialised_term, '*')) {
				serialised_term.pop_back();
				return Xapian::Query(Xapian::Query::OP_WILDCARD, prefixed(serialised_term, field_spc.prefix, field_spc.get_ctype()), wqf);
			} else if (is_wildcard) {
				return Xapian::Query(Xapian::Query::OP_WILDCARD, prefixed(serialised_term, field_spc.prefix, field_spc.get_ctype()), wqf);
			} else {
				return Xapian::Query(prefixed(serialised_term, field_spc.prefix, field_spc.get_ctype()), wqf);
			}
		}

		default:
			return Xapian::Query(prefixed(serialised_term, field_spc.prefix, field_spc.get_ctype()), wqf);
	}
}


Xapian::Query
QueryDSL::get_in_query(const required_spc_t& field_spc, const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_in_query(<field_spc>, %s)", repr(obj.to_string()).c_str());

	if (obj.is_map() && obj.size() == 1) {
		const auto it = obj.begin();
		const auto field_name = it->as_string();
		if (field_name.compare(QUERYDSL_RANGE) == 0) {
			const auto& value = it.value();
			if (value.is_map()) {
				return MultipleValueRange::getQuery(field_spc, value);
			} else {
				THROW(QueryDslError, "%s must be object [%s]", field_name.c_str(), repr(value.to_string()).c_str());
			}
		} else {
			switch ((Cast::Hash)xxh64::hash(field_name)) {
				case Cast::Hash::EWKT:
				case Cast::Hash::POINT:
				case Cast::Hash::CIRCLE:
				case Cast::Hash::CONVEX:
				case Cast::Hash::POLYGON:
				case Cast::Hash::CHULL:
				case Cast::Hash::MULTIPOINT:
				case Cast::Hash::MULTICIRCLE:
				case Cast::Hash::MULTIPOLYGON:
				case Cast::Hash::MULTICHULL:
				case Cast::Hash::GEO_COLLECTION:
				case Cast::Hash::GEO_INTERSECTION:
					return GeoSpatialRange::getQuery(field_spc, obj);
				default:
					THROW(QueryDslError, "Invalid format %s: %s", QUERYDSL_IN, repr(obj.to_string()).c_str());
			}
		}
	} else {
		THROW(QueryDslError, "%s must be object and only contains one element [%s]", QUERYDSL_IN, repr(obj.to_string()).c_str());
	}
}


MsgPack
QueryDSL::make_dsl_query(const query_field_t& e)
{
	L_CALL(this, "Query::make_dsl_query(<query_field_t>)");

	MsgPack dsl(MsgPack::Type::MAP);
	if (e.query.size() == 1) {
		dsl = make_dsl_query(*e.query.begin());
	} else {
		for (const auto& query : e.query) {
			dsl["_and"].push_back(make_dsl_query(query));
		}
	}
	return dsl;
}


MsgPack
QueryDSL::make_dsl_query(const std::string& query)
{
	L_CALL(this, "Query::make_dsl_query(%s)", repr(query).c_str());

	if (query.compare("*") == 0) {
		return "*";
	}

	try {
		BooleanTree booltree(query);
		std::vector<MsgPack> stack_msgpack;

		while (!booltree.empty()) {
			const auto& token = booltree.front();

			switch (token.get_type()) {
				case TokenType::Not:
					if (stack_msgpack.size() < 1) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						MsgPack object = {{ "_not", stack_msgpack.back() }}; // expression.
						stack_msgpack.pop_back();
						stack_msgpack.push_back(std::move(object));
					}
					break;

				case TokenType::Or:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						MsgPack object;
						auto& _or = object["_or"] = { stack_msgpack.back() }; // letf expression
						stack_msgpack.pop_back();
						_or.push_back(stack_msgpack.back()); // right expression
						stack_msgpack.pop_back();
						stack_msgpack.push_back(std::move(object));
					}
					break;

				case TokenType::And:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						MsgPack object;
						auto& _and = object["_and"] = { stack_msgpack.back() }; // letf expression
						stack_msgpack.pop_back();
						_and.push_back(stack_msgpack.back()); // right expression
						stack_msgpack.pop_back();
						stack_msgpack.push_back(std::move(object));
					}
					break;

				case TokenType::Xor:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						MsgPack object;
						auto& _xor = object["_xor"] = { stack_msgpack.back() }; // letf expression
						stack_msgpack.pop_back();
						_xor.push_back(stack_msgpack.back()); // right expression
						stack_msgpack.pop_back();
						stack_msgpack.push_back(std::move(object));
					}
					break;

				case TokenType::Id:	{
					FieldParser fp(token.get_lexeme());
					fp.parse();

					MsgPack value;
					if (fp.is_range()) {
						value[QUERYDSL_IN] = fp.get_values();
					} else {
						value = fp.get_value();
					}

					auto field_name = fp.get_field_name();

					MsgPack object;
					if (field_name.empty()) {
						object[QUERYDSL_RAW] = value;
					} else {
						object[field_name][QUERYDSL_RAW] = value;
					}
					stack_msgpack.push_back(std::move(object));
					break;
				}

				default:
					break;
			}

			booltree.pop_front();
		}

		if (stack_msgpack.size() == 1) {
			return stack_msgpack.back();
		} else {
			THROW(QueryDslError, "Bad boolean expression");
		}
	} catch (const LexicalException& err) {
		THROW(QueryDslError, err.what());
	} catch (const SyntacticException& err) {
		THROW(QueryDslError, err.what());
	}
}


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_query(%s)", repr(obj.to_string()).c_str());

	if (obj.is_string() && obj.as_string().compare("*") == 0) {
		return Xapian::Query::MatchAll;
	}

	auto query = process(Xapian::Query::OP_AND, std::string(), obj, 1, Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD, false, false, false);
	L_QUERY(this, "query = " CYAN "%s" NO_COL "\n" DARK_GREY "%s" NO_COL, query.get_description().c_str(), repr(query.serialise()).c_str());
	return query;
}


#ifdef L_QUERY_DEFINED
#undef L_QUERY_DEFINED
#undef L_QUERY
#endif
