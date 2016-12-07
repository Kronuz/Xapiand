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
#include "exception.h"                         // for THROW, ClientError
#include "field_parser.h"                      // for FieldParser
#include "geo/wkt_parser.h"                    // for EWKT_Parser
#include "log.h"                               // for Log, L_CALL, L
#include "multivalue/generate_terms.h"         // for GenerateTerms
#include "multivalue/range.h"                  // for MultipleValueRange
#include "serialise.h"                         // for MsgPack, get_range_type...
#include "utils.h"                             // for repr, startswith


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
	{ RESERVED_FLOAT,             }, { RESERVED_POSITIVE,          },
	{ RESERVED_INTEGER,           }, { RESERVED_BOOLEAN,           },
	{ RESERVED_TERM,              }, { RESERVED_TEXT,              },
	{ RESERVED_DATE,              }, { RESERVED_UUID,              },
	{ RESERVED_EWKT,              }, { RESERVED_POINT,             },
	{ RESERVED_POLYGON,           }, { RESERVED_CIRCLE,            },
	{ RESERVED_CHULL,             }, { RESERVED_MULTIPOINT,        },
	{ RESERVED_MULTIPOLYGON,      }, { RESERVED_MULTICIRCLE,       },
	{ RESERVED_MULTICHULL,        }, { RESERVED_GEO_COLLECTION,    },
	{ RESERVED_GEO_INTERSECTION,  },
});


/* A domain-specific language (DSL) for query */
QueryDSL::QueryDSL(std::shared_ptr<Schema> schema_)
	: schema(schema_) { }


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
						THROW(ClientError, "Bad boolean expression");
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
						THROW(ClientError, "Bad boolean expression");
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
						THROW(ClientError, "Bad boolean expression");
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
						THROW(ClientError, "Bad boolean expression");
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
					if (fp.isrange) {
						value["_in"] = fp.get_value();
					} else {
						value = fp.get_value();
					}

					auto field_name = fp.get_field();
					if (field_name.empty()) {
						field_name = "_value";
					}
					object[field_name] = value;

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
			THROW(ClientError, "Bad boolean expression");
		}
	} catch (const LexicalException& err) {
		THROW(ClientError, err.what());
	} catch (const SyntacticException& err) {
		THROW(ClientError, err.what());
	}
}


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_query(%s)", obj.to_string().c_str());

	if (obj.is_string() && obj.to_string().compare("*")) {
		return Xapian::Query::MatchAll;
	}

	return process(Xapian::Query::OP_AND, "", obj);
}


Xapian::Query
QueryDSL::process(Xapian::Query::op op, const std::string& parent, const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::process(%d, %s, %s)", (int)op, repr(parent).c_str(), obj.to_string().c_str());

	Xapian::termcount wqf = 1;
	int q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD;

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}

	switch (obj.getType()) {
		case MsgPack::Type::MAP:
			for (auto const& field : obj) {
				auto field_name = field.as_string();
				auto const& o = obj.at(field);

				Xapian::Query query;

				if (field_name == RESERVED_VALUE) {
					query = get_value_query(parent, o, wqf, q_flags);

				} else {
					auto it = ops_map.find(field_name);
					if (it != ops_map.end()) {
						query = process(it->second, parent, o);
					} else if (casts_set.find(field_name) != casts_set.end()) {
						query = get_value_query(parent, {{ field_name, o }}, wqf, q_flags);
					} else {
						query = process(op, parent.empty() ? field_name : parent + "." + field_name, o);
					}
				}

				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;

		case MsgPack::Type::ARRAY:
			for (auto const& o : obj) {
				auto query = process(op, parent, o);
				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;

		default:
			final_query = get_value_query(parent, obj, wqf, q_flags);
			break;
	}

	return final_query;
}


Xapian::Query
QueryDSL::get_value_query(const std::string& path, const MsgPack& obj, Xapian::termcount wqf, int q_flags)
{
	L_CALL(this, "QueryDSL::get_value_query(%s, %s)", repr(path).c_str(), obj.to_string().c_str());

	if (path.empty()) {
		auto ser_type = Serialise::get_type(obj);
		const auto& global_spc = Schema::get_data_global(std::get<0>(ser_type));

		return get_regular_query(global_spc, obj, wqf, q_flags);
	} else {
		auto data_field = schema->get_data_field(path);
		const auto& field_spc = data_field.first;

		if (!data_field.second.empty()) {
			return get_accuracy_query(data_field.second, field_spc.prefix, obj);
		}

		if (field_spc.flags.inside_namespace) {
			return get_namespace_query(path, field_spc.prefix, obj, q_flags);
		}

		return get_regular_query(field_spc, obj, wqf, q_flags);
	}
}


Xapian::Query
QueryDSL::get_accuracy_query(const std::string& field_accuracy, const std::string& prefix_accuracy, const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_accuracy_query(%s, %s, %s)", repr(field_accuracy).c_str(), repr(prefix_accuracy).c_str(), obj.to_string().c_str());

	// if (fp.isrange) {
	// 	THROW(ClientError, "Accuracy is only indexed like terms, searching by range is not supported");
	// }

	auto it = map_acc_date.find(field_accuracy.substr(1));
	if (it != map_acc_date.end()) {
		// Check it is date accuracy.
		Datetime::tm_t tm = Datetime::to_tm_t(obj);
		static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE));
		switch (it->second) {
			case UnitTime::SECOND: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::MINUTE: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::HOUR: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::DAY: {
				Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::MONTH: {
				Datetime::tm_t _tm(tm.year, tm.mon);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::YEAR: {
				Datetime::tm_t _tm(tm.year);
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::DECADE: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::CENTURY: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
			case UnitTime::MILLENNIUM: {
				Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
				return Xapian::Query(prefixed(Serialise::serialise(_tm), prefix_type + prefix_accuracy));
			}
		}
	} else {
		try {
			if (field_accuracy.find("_geo") == 0) {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO));
				auto nivel = stox(std::stoull, field_accuracy.substr(4));
				auto value = Cast::string(obj);  // TODO: use Cast::geo() instead?
				EWKT_Parser ewkt(value, DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR);
				auto ranges = ewkt.getRanges();
				return GenerateTerms::geo(ranges, { nivel }, { prefix_type + prefix_accuracy } );
			} else {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER));
				auto acc = stox(std::stoull, field_accuracy.substr(1));
				auto value = Cast::integer(obj);
				return Xapian::Query(prefixed(Serialise::integer(value - GenerateTerms::modulus(value, acc)), prefix_type + prefix_accuracy));
			}
		} catch (const std::invalid_argument& e) {
			THROW(ClientError, "Invalid numeric value %s: %s [%s]", field_accuracy.c_str(), obj.to_string().c_str(), e.what());
		}
	}
}


Xapian::Query
QueryDSL::get_namespace_query(const std::string& full_name, const std::string& prefix_namespace, const MsgPack& obj, int q_flags)
{
	L_CALL(this, "QueryDSL::get_namespace_query(%s, %s, %s)", repr(full_name).c_str(), repr(prefix_namespace).c_str(), obj.to_string().c_str());

	std::string f_prefix;
	f_prefix.reserve(arraySize(DOCUMENT_NAMESPACE_TERM_PREFIX) + prefix_namespace.length() + 1);
	f_prefix.assign(DOCUMENT_NAMESPACE_TERM_PREFIX).append(prefix_namespace);

	// if (fp.isrange) {
	// 	auto ser_type = Serialise::get_range_type(fp.start, fp.end);
	// 	auto spc = Schema::get_namespace_specification(std::get<0>(ser_type), f_prefix);
	// 	return MultipleValueRange::getQuery(spc, full_name, fp.start, fp.end);
	// }

	auto ser_type = Serialise::get_type(obj);
	auto& field_value = std::get<1>(ser_type);

	if (field_value.empty()) {
		return Xapian::Query(f_prefix);
	} else if (field_value == "*") {
		return Xapian::Query(Xapian::Query::OP_WILDCARD, f_prefix);
	}

	auto spc = Schema::get_namespace_specification(std::get<0>(ser_type), f_prefix);
	field_value.assign(std::get<1>(ser_type));

	switch (spc.get_type()) {
		case FieldType::TEXT: {
			Xapian::QueryParser parser;
			// parser.set_database(*database->db);
			auto stopper = getStopper(spc.language);
			parser.set_stopper(stopper.get());
			parser.set_stemming_strategy(getQueryParserStemStrategy(spc.stem_strategy));
			parser.set_stemmer(Xapian::Stem(spc.stem_language));
			if (spc.flags.bool_term) {
				parser.add_boolean_prefix("_", spc.prefix);
			} else {
				parser.add_prefix("_", spc.prefix);
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
			return Xapian::Query(prefixed(field_value, spc.prefix));
	}
}


Xapian::Query
QueryDSL::get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, int q_flags)
{
	L_CALL(this, "QueryDSL::get_regular_query(<field_spc>, %s)", obj.to_string().c_str());

	auto field_value = Serialise::MsgPack(field_spc, obj);

	switch (field_spc.get_type()) {
		case FieldType::TEXT: {
			Xapian::QueryParser parser;
			if (field_spc.flags.bool_term) {
				parser.add_boolean_prefix("_", field_spc.prefix);
			} else {
				parser.add_prefix("_", field_spc.prefix);
			}
			auto stopper = getStopper(field_spc.language);
			parser.set_stopper(stopper.get());
			parser.set_stemming_strategy(getQueryParserStemStrategy(field_spc.stem_strategy));
			parser.set_stemmer(Xapian::Stem(field_spc.stem_language));
			return parser.parse_query("_:" + field_value, q_flags);
		}

		case FieldType::STRING: {
			Xapian::QueryParser parser;
			if (field_spc.flags.bool_term) {
				parser.add_boolean_prefix("_", field_spc.prefix);
			} else {
				parser.add_prefix("_", field_spc.prefix);
			}
			return parser.parse_query("_:" + field_value, q_flags);
		}

		case FieldType::TERM: {
			if (!field_spc.flags.bool_term) {
				to_lower(field_value);
			}
			if (endswith(field_value, '*')) {
				field_value = field_value.substr(0, field_value.length() - 1);
				return Xapian::Query(Xapian::Query::OP_WILDCARD, prefixed(field_value, field_spc.prefix));
			} else {
				return Xapian::Query(prefixed(field_value, field_spc.prefix), wqf);
			}
		}

		default:
			return Xapian::Query(prefixed(field_value, field_spc.prefix), wqf);
	}
}
