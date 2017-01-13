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

#include "query_dsl.h"

#include "booleanParser/BooleanParser.h"       // for BooleanTree
#include "booleanParser/LexicalException.h"    // for LexicalException
#include "booleanParser/SyntacticException.h"  // for SyntacticException
#include "database_utils.h"                    // for prefixed, RESERVED_VALUE
#include "exception.h"                         // for THROW, QueryDslError
#include "field_parser.h"                      // for FieldParser
#include "geo/wkt_parser.h"                    // for EWKT_Parser
#include "log.h"                               // for Log, L_CALL, L
#include "multivalue/generate_terms.h"         // for GenerateTerms
#include "multivalue/range.h"                  // for MultipleValueRange
#include "serialise.h"                         // for MsgPack, get_range_type...
#include "utils.h"                             // for repr, startswith


#ifndef L_QUERY
#define L_QUERY_DEFINED
#define L_QUERY(args...)
#endif


const std::unordered_map<std::string, Xapian::Query::op> QueryDSL::ops_map({
	{ "_and",           Xapian::Query::OP_AND           },
	{ "_or",            Xapian::Query::OP_OR            },
	{ "_and_not",       Xapian::Query::OP_AND_NOT       },
	{ "_not",           Xapian::Query::OP_AND_NOT       },
	{ "_xor",           Xapian::Query::OP_XOR           },
	{ "_and_maybe",     Xapian::Query::OP_AND_MAYBE     },
	{ "_filter",        Xapian::Query::OP_FILTER        },
	{ "_near",          Xapian::Query::OP_NEAR          },
	{ "_phrase",        Xapian::Query::OP_PHRASE        },
	{ "_value_range",   Xapian::Query::OP_VALUE_RANGE   },
	{ "_scale_weight",  Xapian::Query::OP_SCALE_WEIGHT  },
	{ "_elite_set",     Xapian::Query::OP_ELITE_SET     },
	{ "_value_ge",      Xapian::Query::OP_VALUE_GE      },
	{ "_value_le",      Xapian::Query::OP_VALUE_LE      },
	{ "_synonym",       Xapian::Query::OP_SYNONYM       },
	{ "_max",           Xapian::Query::OP_MAX           },
	{ "_wildcard",      Xapian::Query::OP_WILDCARD      },
});


const std::unordered_set<std::string> QueryDSL::casts_set({
	RESERVED_FLOAT,              RESERVED_POSITIVE,
	RESERVED_INTEGER,            RESERVED_BOOLEAN,
	RESERVED_TERM,               RESERVED_TEXT,
	RESERVED_DATE,               RESERVED_UUID,
	RESERVED_EWKT,               RESERVED_POINT,
	RESERVED_POLYGON,            RESERVED_CIRCLE,
	RESERVED_CHULL,              RESERVED_MULTIPOINT,
	RESERVED_MULTIPOLYGON,       RESERVED_MULTICIRCLE,
	RESERVED_MULTICHULL,         RESERVED_GEO_COLLECTION,
	RESERVED_GEO_INTERSECTION,
});


/* A domain-specific language (DSL) for query */


QueryDSL::QueryDSL(const std::shared_ptr<Schema>& schema_)
	: schema(schema_) { }


FieldType
QueryDSL::get_in_type(const MsgPack& obj)
{
	if (obj.find("_range") != obj.end()) {
		const auto& range = obj.at("_range");
		if (range.find("_from") != range.end()) {
			return std::get<0>(Serialise::get_type(range.at("_from")));
		} else if (range.find("_to") != range.end()) {
			return std::get<0>(Serialise::get_type(range.at("_to")));
		}
	}
	return FieldType::EMPTY;
}


std::pair<FieldType, MsgPack>
QueryDSL::parse_range(const required_spc_t& field_spc, const std::string& range)
{
	FieldParser fp(range);
	fp.parse();
	if (!fp.is_range()) {
		THROW(QueryDslError, "Invalid range (1): %s", range.c_str());
	}
	MsgPack value;
	auto& _range = value["_range"];
	auto start = fp.get_start();
	FieldType field_type = FieldType::EMPTY;
	if (!start.empty()) {
		auto& obj = _range["_from"] = Cast::cast(field_spc.get_type(), start);
		field_type = std::get<0>(Serialise::get_type(obj));
	}
	auto end = fp.get_end();
	if (!end.empty()) {
		auto& obj = _range["_to"] = Cast::cast(field_spc.get_type(), end);
		if (field_type == FieldType::EMPTY) {
			field_type = std::get<0>(Serialise::get_type(obj));
		}
	}

	return std::make_pair(field_type, value);
}


Xapian::Query
QueryDSL::process(Xapian::Query::op op, const std::string& parent, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in)
{
	L_CALL(this, "QueryDSL::process(%d, %s, %s)", (int)op, repr(parent).c_str(), repr(obj.to_string()).c_str());

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}

	switch (obj.getType()) {
		case MsgPack::Type::MAP:
			for (auto const& field : obj) {
				auto field_name = field.as_string();
				auto const& o = obj.at(field_name);

				L_QUERY(this, BLUE "%s = %s" NO_COL, field_name.c_str(), o.to_string().c_str());

				Xapian::Query query;

				if (field_name == RESERVED_RAW) {
					query = process(op, parent, o, wqf, q_flags, true, is_in);
				} else if (field_name == RESERVED_IN) {
					query = process(op, parent, o, wqf, q_flags, is_raw, true);
				} else if (field_name == RESERVED_VALUE) {
					query = get_value_query(op, parent, o, wqf, q_flags, is_raw, is_in);
				} else {
					auto it = ops_map.find(field_name);
					if (it != ops_map.end()) {
						query = process(it->second, parent, o, wqf, q_flags, is_raw, is_in);
					} else if (casts_set.find(field_name) != casts_set.end()) {
						query = get_value_query(op, parent, {{ field_name, o }}, wqf, q_flags, is_raw, is_in);
					} else {
						query = process(op, parent.empty() ? field_name : parent + "." + field_name, o, wqf, q_flags, is_raw, is_in);
					}
				}

				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;

		case MsgPack::Type::ARRAY:
			for (auto const& o : obj) {
				auto query = process(op, parent, o, wqf, q_flags, is_raw, is_in);
				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;

		default:
			final_query = get_value_query(op, parent, obj, wqf, q_flags, is_raw, is_in);
			break;
	}

	return final_query;
}


Xapian::Query
QueryDSL::get_value_query(Xapian::Query::op op, const std::string& path, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in)
{
	L_CALL(this, "QueryDSL::get_value_query(%d, %s, %s, <wqf>, <q_flags>, %s, %s)", (int)op, repr(path).c_str(), repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false");

	if (path.empty()) {
		FieldType field_type;
		MsgPack aux;
		const MsgPack* pobj;
		if (is_raw && obj.is_string()) {
			if (is_in) {
				field_type = FieldType::TEXT;
				pobj = &obj;
			} else {
				aux = Cast::cast(FieldType::EMPTY, obj.as_string());
				field_type = std::get<0>(Serialise::get_type(aux));
				pobj = &aux;
			}
		} else {
			field_type = std::get<0>(Serialise::get_type(obj));
			pobj = &obj;
		}
		const auto& global_spc = Schema::get_data_global(field_type);
		return get_regular_query(global_spc, op, *pobj, wqf, q_flags, is_raw, is_in);
	} else {
		auto data_field = schema->get_data_field(path, is_in);
		const auto& field_spc = data_field.first;

		if (!data_field.second.empty()) {
			return get_accuracy_query(field_spc, op, data_field.second, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, is_raw, is_in);
		}

		if (field_spc.flags.inside_namespace) {
			return get_namespace_query(field_spc, op, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, wqf, q_flags, is_raw, is_in);
		}

		return get_regular_query(field_spc, op, (!is_in && is_raw && obj.is_string()) ? Cast::cast(field_spc.get_type(), obj.as_string()) : obj, wqf, q_flags, is_raw, is_in);
	}
}


Xapian::Query
QueryDSL::get_acc_date_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj)
{
	auto it = map_acc_date.find(field_accuracy.substr(1));
	if (it != map_acc_date.end()) {
		Datetime::tm_t tm = Datetime::to_tm_t(obj);
		switch (it->second) {
			case UnitTime::SECOND: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::MINUTE: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::HOUR: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::DAY: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::MONTH: {
				Datetime::tm_t _tm(tm.year, tm.mon);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::YEAR: {
				Datetime::tm_t _tm(tm.year);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::DECADE: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::CENTURY: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
			case UnitTime::MILLENNIUM: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix, toUType(FieldType::DATE)));
			}
		}
	}

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
}


Xapian::Query
QueryDSL::get_acc_num_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj)
{
	try {
		auto acc = stox(std::stoull, field_accuracy.substr(1));
		auto value = Cast::integer(obj);
		return Xapian::Query(prefixed(Serialise::integer(value - modulus(value, acc)), field_spc.prefix, toUType(FieldType::INTEGER)));
	} catch (const InvalidArgument&) {
	} catch (const OutOfRange&) { }

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
}


Xapian::Query
QueryDSL::get_acc_geo_query(const required_spc_t& field_spc, const std::string& field_accuracy, const MsgPack& obj)
{
	if (field_accuracy.find("_geo") == 0) {
		try {
			auto nivel = stox(std::stoull, field_accuracy.substr(4));
			auto value = Cast::string(obj);  // FIXME: use Cast::geo() instead?
			EWKT_Parser ewkt(value, default_spc.flags.partials, default_spc.error);
			auto ranges = ewkt.getRanges();
			return GenerateTerms::geo(ranges, { nivel }, { field_spc.prefix });
		} catch (const InvalidArgument&) {
		} catch (const OutOfRange&) { }
	}

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
}


Xapian::Query
QueryDSL::get_accuracy_query(const required_spc_t& field_spc, Xapian::Query::op op, const std::string& field_accuracy, const MsgPack& obj, bool is_raw, bool is_in)
{
	L_CALL(this, "QueryDSL::get_accuracy_query(<field_spc>, %d, %s, %s, %s, %s)", (int)op, repr(field_accuracy).c_str(), repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false"); (void)op; (void)is_raw;

	if (is_in) {
		THROW(QueryDslError, "Accuracy is only indexed like terms, searching by range is not supported");
	}

	if (field_spc.flags.inside_namespace) {
		// Check if accuracy field is date.
		try {
			return get_acc_date_query(field_spc, field_accuracy, obj);
		} catch (const QueryDslError&) { }

		// Check if accuracy field is num.
		try {
			return get_acc_num_query(field_spc, field_accuracy, obj);
		} catch (const QueryDslError&) { }

		// Accuracy field must be geo.
		return get_acc_geo_query(field_spc, field_accuracy, obj);
	} else {
		switch (field_spc.get_type()) {
			case FieldType::INTEGER:
			case FieldType::POSITIVE:
			case FieldType::FLOAT:
				return get_acc_num_query(field_spc, field_accuracy, obj);
			case FieldType::DATE:
				return get_acc_date_query(field_spc, field_accuracy, obj);
			case FieldType::GEO:
				return get_acc_geo_query(field_spc, field_accuracy, obj);
			default:
				THROW(QueryDslError, "Invalid field name: %s", field_accuracy.c_str());
		}
	}
}


Xapian::Query
QueryDSL::get_namespace_query(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in)
{
	L_CALL(this, "QueryDSL::get_namespace_query(<field_spc>, %d, %s, <wqf>, <q_flags>, %s, %s)", (int)op, repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false");

	if (is_in) {
		if (obj.is_string()) {
			auto parsed = parse_range(field_spc, obj.as_string());
			auto spc = Schema::get_namespace_specification(parsed.first, field_spc.prefix);
			return get_in_query(spc, op, parsed.second, wqf, q_flags, is_raw, is_in);
		} else {
			auto spc = Schema::get_namespace_specification(get_in_type(obj), field_spc.prefix);
			return get_in_query(spc, op, obj, wqf, q_flags, is_raw, is_in);
		}
	}

	if (obj.is_string()) {
		auto val = obj.as_string();
		if (val.empty()) {
			return Xapian::Query(field_spc.prefix);
		} else if (val == "*") {
			return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix);
		}
	}

	auto ser_type = Serialise::get_type(obj);
	auto spc = Schema::get_namespace_specification(std::get<0>(ser_type), field_spc.prefix);

	auto& field_value = std::get<1>(ser_type);

	switch (spc.get_type()) {
		case FieldType::TEXT: {
			Xapian::QueryParser parser;
			// parser.set_database(*database->db);
			const auto& stopper = getStopper(spc.language);
			parser.set_stopper(stopper.get());
			parser.set_stemming_strategy(getQueryParserStemStrategy(spc.stem_strategy));
			parser.set_stemmer(Xapian::Stem(spc.stem_language));
			if (spc.flags.bool_term) {
				parser.add_boolean_prefix("_", spc.prefix + spc.get_ctype());
			} else {
				parser.add_prefix("_", spc.prefix + spc.get_ctype());
			}
			return parser.parse_query("_:" + field_value, q_flags);
		}
		case FieldType::STRING: {
			Xapian::QueryParser parser;
			// parser.set_database(*database->db);
			if (spc.flags.bool_term) {
				parser.add_boolean_prefix("_", spc.prefix);
			} else {
				parser.add_prefix("_", spc.prefix);
			}
			return parser.parse_query("_:" + field_value, q_flags);
		}
		default:
			return Xapian::Query(prefixed(field_value, spc.prefix, spc.get_ctype()));
	}
}


Xapian::Query
QueryDSL::get_regular_query(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj, Xapian::termcount wqf, int q_flags, bool is_raw, bool is_in)
{
	L_CALL(this, "QueryDSL::get_regular_query(<field_spc>, %d, %s, <wqf>, <q_flags>, %s, %s)", (int)op, repr(obj.to_string()).c_str(), is_raw ? "true" : "false", is_in ? "true" : "false");

	if (is_in) {
		if (obj.is_string()) {
			auto parsed = parse_range(field_spc, obj.as_string());
			return get_in_query(field_spc, op, parsed.second, wqf, q_flags, is_raw, is_in);
		}
		return get_in_query(field_spc, op, obj, wqf, q_flags, is_raw, is_in);
	}

	if (obj.is_string()) {
		auto val = obj.as_string();
		if (val.empty()) {
			return Xapian::Query(field_spc.prefix);
		} else if (val == "*") {
			return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix);
		}
	}

	switch (field_spc.get_type()) {
		case FieldType::TEXT: {
			auto field_value = Serialise::MsgPack(field_spc, obj);
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
			return parser.parse_query("_:" + field_value, q_flags);
		}

		case FieldType::STRING: {
			auto field_value = Serialise::MsgPack(field_spc, obj);
			Xapian::QueryParser parser;
			if (field_spc.flags.bool_term) {
				parser.add_boolean_prefix("_", field_spc.prefix + field_spc.get_ctype());
			} else {
				parser.add_prefix("_", field_spc.prefix + field_spc.get_ctype());
			}
			return parser.parse_query("_:" + field_value, q_flags);
		}

		case FieldType::TERM: {
			auto field_value = Serialise::MsgPack(field_spc, obj);
			if (!field_spc.flags.bool_term) {
				to_lower(field_value);
			}
			if (endswith(field_value, '*')) {
				field_value = field_value.substr(0, field_value.length() - 1);
				return Xapian::Query(Xapian::Query::OP_WILDCARD, prefixed(field_value, field_spc.prefix, field_spc.get_ctype()));
			} else {
				return Xapian::Query(prefixed(field_value, field_spc.prefix, field_spc.get_ctype()), wqf);
			}
		}

		case FieldType::EMPTY:
			return Xapian::Query();

		default:
			auto field_value = Serialise::MsgPack(field_spc, obj);
			return Xapian::Query(prefixed(field_value, field_spc.prefix, field_spc.get_ctype()), wqf);
	}
}


Xapian::Query
QueryDSL::get_in_query(const required_spc_t& field_spc, Xapian::Query::op op, const MsgPack& obj, Xapian::termcount, int, bool, bool)
{
	L_CALL(this, "QueryDSL::get_in_query(<field_spc>, %d, %s)", (int)op, repr(obj.to_string()).c_str());

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}

	for (auto const& field : obj) {
		auto field_name = field.as_string();
		auto const& o = obj.at(field);
		Xapian::Query query;
		if (field_name.compare("_range") == 0) {
			query = MultipleValueRange::getQuery(field_spc, o);
		} else {
			THROW(QueryDslError, "Invalid _in: %s", repr(obj.to_string()).c_str());
		}
		final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
	}

	return final_query;
}


MsgPack
QueryDSL::make_dsl_query(const query_field_t& e)
{
	L_CALL(this, "Query::make_dsl_query()");

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
			auto token = booltree.front();
			booltree.pop_front();

			switch (token.type) {
				case TokenType::Not:
					if (stack_msgpack.size() < 1) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						auto expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						MsgPack object(MsgPack::Type::MAP);
						object["_not"] = { expression };
						stack_msgpack.push_back(object);
					}
					break;

				case TokenType::Or:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						auto letf_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto right_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						MsgPack object(MsgPack::Type::MAP);
						object["_or"] = { letf_expression, right_expression };
						stack_msgpack.push_back(object);
					}
					break;

				case TokenType::And:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						auto letf_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto right_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						MsgPack object(MsgPack::Type::MAP);
						object["_and"] = { letf_expression, right_expression };
						stack_msgpack.push_back(object);
					}
					break;

				case TokenType::Xor:
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					} else {
						auto letf_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto right_expression = stack_msgpack.back();
						stack_msgpack.pop_back();
						MsgPack object(MsgPack::Type::MAP);
						object["_xor"] = { letf_expression, right_expression };
						stack_msgpack.push_back(object);
					}
					break;

				case TokenType::Id:	{
					MsgPack object(MsgPack::Type::MAP);
					FieldParser fp(token.lexeme);
					fp.parse();

					MsgPack value;
					if (fp.is_range()) {
						value[RESERVED_IN] = fp.get_values();
					} else {
						value = fp.get_value();
					}

					auto field_name = fp.get_field_name();
					if (field_name.empty()) {
						object[RESERVED_RAW] = value;
					} else {
						object[field_name][RESERVED_RAW] = value;
					}

					stack_msgpack.push_back(object);
					break;
				}

				default:
					break;
			}
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

	auto query = process(Xapian::Query::OP_AND, "", obj, 1, Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD, false, false);
	L_QUERY(this, "query = " CYAN "%s" NO_COL "\n" DARK_GREY "%s" NO_COL, query.get_description().c_str(), repr(query.serialise()).c_str());
	return query;
}


#ifdef L_QUERY_DEFINED
#undef L_QUERY_DEFINED
#undef L_QUERY
#endif
