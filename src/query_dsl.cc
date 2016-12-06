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

#include <stdexcept>                           // for out_of_range
#include <tuple>                               // for get, tuple

#include "booleanParser/BooleanParser.h"       // for BooleanTree
#include "booleanParser/LexicalException.h"    // for LexicalException
#include "booleanParser/SyntacticException.h"  // for SyntacticException
#include "database_utils.h"                    // for prefixed, RESERVED_BOOLEAN, RESERV...
#include "exception.h"                         // for MSG_QueryDslError, QueryDslError
#include "log.h"                               // for Log, L_CALL, L
#include "msgpack/object_fwd.hpp"              // for type_error
#include "multivalue/range.h"                  // for MultipleValueRange
#include "schema.h"                            // for FieldType, required_spc_t, FieldTy...
#include "serialise.h"                         // for MsgPack, date, get_range_type, get...
#include "utils.h"                             // for repr, lower_string, startswith
#include "xxh64.hpp"                           // for xxh64



/* Reserved DSL words used for operators */
constexpr const char QUERYDSL_OR[]        = "_or";
constexpr const char QUERYDSL_AND[]       = "_and";
constexpr const char QUERYDSL_XOR[]       = "_xor";
constexpr const char QUERYDSL_NOT[]       = "_not";

/* Reserved DSL words used for values */
constexpr const char QUERYDSL_VALUE[]     = "_value";
constexpr const char QUERYDSL_IN[]        = "_in";

/* Reserved DSL words used for parameters */
constexpr const char QUERYDSL_BOOST[]     = "_boost";

/* Reserved DSL words used for Match all query */
constexpr const char QUERYDSL_MATCH_ALL[] = "_all";

/* Reserved DSL words used for set partial search */
constexpr const char QUERYDSL_PARTIAL[] = "_partial";

/* Reserved DSL words used for dates */
constexpr const char QUERYDSL_YEAR[]      = "_year";
constexpr const char QUERYDSL_MOTH[]      = "_moth";
constexpr const char QUERYDSL_DAY[]       = "_day";
constexpr const char QUERYDSL_TIME[]      = "_time";

/* Reserved DSL words used for ranges */
constexpr const char QUERYDSL_RANGE[]     = "_range";
//constexpr const char QUERYDSL_GEO_POLIGON[] = "_polygon";


static constexpr auto HASH_ALL = xxh64::hash(QUERYDSL_MATCH_ALL);


const std::unordered_map<std::string, Xapian::Query::op> map_xapian_operator({
	{ QUERYDSL_OR,        Xapian::Query::OP_OR         },
	{ QUERYDSL_AND,       Xapian::Query::OP_AND        },
	{ QUERYDSL_XOR,       Xapian::Query::OP_XOR  	   },
	{ QUERYDSL_NOT,       Xapian::Query::OP_AND_NOT    },
});


const std::unordered_map<std::string, QueryDSL::dispatch_op_dsl> QueryDSL::map_op_dispatch_dsl({
	{ QUERYDSL_OR,        &QueryDSL::join_queries         },
	{ QUERYDSL_AND,       &QueryDSL::join_queries         },
	{ QUERYDSL_XOR,       &QueryDSL::join_queries  		  },
	{ QUERYDSL_NOT,       &QueryDSL::join_queries         },
});


const std::unordered_map<std::string, QueryDSL::dispatch_dsl> QueryDSL::map_dispatch_dsl({
	{ QUERYDSL_IN,            &QueryDSL::in_range_query   },
	{ QUERYDSL_VALUE,         &QueryDSL::query            },
});


const std::unordered_map<std::string, QueryDSL::dispatch_dsl> QueryDSL::map_dispatch_cast({
	{ RESERVED_INTEGER,      &QueryDSL::query          },
	{ RESERVED_POSITIVE,     &QueryDSL::query          },
	{ RESERVED_FLOAT,        &QueryDSL::query          },
	{ RESERVED_BOOLEAN,      &QueryDSL::query          },
	{ RESERVED_TERM,         &QueryDSL::query          },
	{ RESERVED_TEXT,         &QueryDSL::query          },
	{ RESERVED_EWKT,         &QueryDSL::query          },
	{ RESERVED_UUID,         &QueryDSL::query          },
	{ RESERVED_DATE,         &QueryDSL::query          },
});


const std::unordered_map<std::string, QueryDSL::dispatch_dsl> QueryDSL::map_range_dispatch_dsl({
	{ QUERYDSL_RANGE,            &QueryDSL::range_query   },
	{ RESERVED_POINT,            &QueryDSL::range_query   },
	{ RESERVED_CIRCLE,           &QueryDSL::range_query   },
	{ RESERVED_CHULL,            &QueryDSL::range_query   },
	{ RESERVED_MULTIPOINT,       &QueryDSL::range_query   },
	{ RESERVED_MULTIPOLYGON,     &QueryDSL::range_query   },
	{ RESERVED_MULTICIRCLE,      &QueryDSL::range_query   },
	{ RESERVED_GEO_COLLECTION,   &QueryDSL::range_query   },
	{ RESERVED_GEO_INTERSECTION, &QueryDSL::range_query   },
	/* Add more types in range */
});


QueryDSL::specification_dsl::specification_dsl()
	: q_flags(Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD),
	  wqf(1) { }


QueryDSL::specification_dsl::specification_dsl(const specification_dsl& o)
	: q_flags(o.q_flags),
      wqf(o.wqf) { }


QueryDSL::specification_dsl::specification_dsl(specification_dsl&& o)
	: q_flags(std::move(o.q_flags)),
      wqf(std::move(o.wqf)) { }


QueryDSL::specification_dsl&
QueryDSL::specification_dsl::operator=(const specification_dsl& o) {
	q_flags = o.q_flags;
	wqf = o.wqf;
	return *this;
}


QueryDSL::specification_dsl&
QueryDSL::specification_dsl::operator=(specification_dsl&& o) noexcept {
	q_flags = std::move(o.q_flags);
	wqf = std::move(o.wqf);
	return *this;
}


/* A domain-specific language (DSL) for query */
QueryDSL::QueryDSL(std::shared_ptr<Schema> schema_)
	: schema(schema_),
	  state(QUERY::INIT) { }


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_query()");

	if (obj.is_map()) {
		for (auto const& elem : obj) {
			state =	QUERY::GLOBALQUERY;
			auto key = elem.as_string();
			Xapian::Query qry;

			set_specifications(obj);
			if (is_reserved(key)) {
				if (find_operators(key, obj.at(key), qry)) {
					return qry;
				} else if (find_values(key, obj, qry)) {
					return qry;
				} else if (find_casts(key, obj, qry)) {
					return qry;
				}
			} else {
				auto const& o = obj.at(key);
				switch (o.getType()) {
					case MsgPack::Type::ARRAY:
						THROW(QueryDslError, "Unexpected type %s in %s", MsgPackTypes[static_cast<int>(MsgPack::Type::ARRAY)], key.c_str());
					case MsgPack::Type::MAP:
						return process_query(o, key);
					default: {
						state =	QUERY::FIELDQUERY;
						fieldname = key;
						return query(o);
					}
				}
			}
		}
	} else if (obj.is_string() && xxh64::hash(lower_string(obj.as_string())) == HASH_ALL) {
		return Xapian::Query::MatchAll;
	} else {
		THROW(QueryDslError, "Type error expected map at root level in query dsl");
	}
	return Xapian::Query();
}


Xapian::Query
QueryDSL::join_queries(const MsgPack& obj, Xapian::Query::op op)
{
	L_CALL(this, "QueryDSL::join_queries()");

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}
	if (obj.is_array()) {
		for (const auto& elem : obj) {
			if (elem.is_map()) {
				set_specifications(elem);

				for (const auto& field : elem) {
					state = QUERY::GLOBALQUERY;
					auto key = field.as_string();
					Xapian::Query qry;
					if (is_reserved(key)) {
						if (find_operators(key, elem.at(key), qry)) {
							final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
						} else if (find_values(key, elem, qry)) {
							final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
						} else if (find_casts(key, elem, qry)) {
							final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
						}
					} else {
						const auto& o = elem.at(key);
						switch (o.getType()) {
							case MsgPack::Type::ARRAY:
								THROW(QueryDslError, "Unexpected type array in %s", key.c_str());
							case MsgPack::Type::MAP:
								final_query.empty() ? final_query = process_query(o, key) : final_query = Xapian::Query(op, final_query, process_query(o, key));
								break;
							default:
								state = QUERY::FIELDQUERY;
								fieldname = key;
								final_query.empty() ? final_query = query(o) : final_query = Xapian::Query(op, final_query, query(o));
						}
					}
				}
			} else {
				THROW(QueryDslError, "Expected array of objects");
			}
		}
	} else {
		THROW(QueryDslError, "Type error expected map in boolean operator");
	}

	clean_specification();
	return final_query;
}


Xapian::Query
QueryDSL::process_query(const MsgPack& obj, const std::string& field_name_)
{
	L_CALL(this, "QueryDSL::process_query(%s)", repr(field_name_).c_str());

	fieldname = field_name_;
	state = QUERY::FIELDQUERY;
	if (obj.is_map()) {
		set_specifications(obj);
		for (const auto& elem : obj) {
			auto key = elem.as_string();
			Xapian::Query qry;

			if (find_values(key, obj, qry)) {
				clean_specification();
				return qry;
			} else if (find_casts(key, obj, qry)) {
				clean_specification();
				return qry;
			}
		}
	}

	clean_specification();
	return Xapian::Query();
}


Xapian::Query
QueryDSL::in_range_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::in_range_query()");

	if (obj.is_map() && obj.size() == 1) {
		for (const auto& elem : obj) {
			auto key = elem.as_string();
			Xapian::Query qry;
			if (find_ranges(key, obj, qry)) {
				return qry;
			} else {
				THROW(QueryDslError, "Unexpected range type %s", key.c_str());
			}
		}
	} else {
		THROW(QueryDslError, "Expected object type with one element");
	}

	return Xapian::Query();
}


Xapian::Query
QueryDSL::range_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::range_query()");

	switch (state) {
		case QUERY::GLOBALQUERY:
		{
			std::tuple<FieldType, std::string, std::string, const required_spc_t&> ser_type = Serialise::get_range_type(obj);
			return MultipleValueRange::getQuery(std::get<3>(ser_type), "", obj);
		}
		break;

		case QUERY::FIELDQUERY:
			return MultipleValueRange::getQuery(schema->get_data_field(fieldname).first, fieldname, obj);

		default:
			break;
	}
	return Xapian::Query();
}


Xapian::Query
QueryDSL::query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::query()");

	auto spc_dsl = specifications.back();
	clean_specification();

	switch (state) {
		case QUERY::GLOBALQUERY:
		{
			auto ser_type = Serialise::get_type(obj);
			switch (std::get<0>(ser_type)) {
				case FieldType::TEXT: {
					Xapian::QueryParser queryTexts;
					auto stopper = getStopper(std::get<2>(ser_type).language);
					queryTexts.set_stopper(stopper.get());
					queryTexts.set_stemming_strategy(getQueryParserStemStrategy(std::get<2>(ser_type).stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(std::get<2>(ser_type).stem_language));
					return queryTexts.parse_query(obj.as_string(), spc_dsl.q_flags);
				}

				case FieldType::STRING: {
					Xapian::QueryParser queryTexts;
					return queryTexts.parse_query(obj.as_string(), spc_dsl.q_flags);
				}

				default:
					return Xapian::Query(prefixed(std::get<1>(ser_type), std::get<2>(ser_type).prefix));
			}
		}
		break;

		case QUERY::FIELDQUERY:
		{
			auto field_spc = schema->get_data_field(fieldname).first;
			if (!field_spc.prefix.empty()){
				try {
					switch (field_spc.get_type()) {

						case FieldType::DATE:
							if (find_date(obj)) {
								auto ser_date = Serialise::date(field_spc, obj);
								return Xapian::Query(prefixed(ser_date, field_spc.prefix), spc_dsl.wqf);
							}

						case FieldType::INTEGER:
						case FieldType::POSITIVE:
						case FieldType::FLOAT:
						case FieldType::UUID:
						case FieldType::BOOLEAN:
							return Xapian::Query(prefixed(Serialise::MsgPack(field_spc, obj), field_spc.prefix), spc_dsl.wqf);

						case FieldType::TEXT: {
							auto field_value = Serialise::MsgPack(field_spc, obj);
							Xapian::QueryParser queryTexts;
							if (field_spc.flags.bool_term) {
								queryTexts.add_boolean_prefix(fieldname, field_spc.prefix);
							} else {
								queryTexts.add_prefix(fieldname, field_spc.prefix);
							}
							auto stopper = getStopper(field_spc.language);
							queryTexts.set_stopper(stopper.get());
							queryTexts.set_stemming_strategy(getQueryParserStemStrategy(field_spc.stem_strategy));
							queryTexts.set_stemmer(Xapian::Stem(field_spc.stem_language));
							std::string str_texts;
							str_texts.reserve(field_value.length() + field_value.length() + 1);
							str_texts.assign(field_value).append(":").append(field_value);
							return queryTexts.parse_query(str_texts, spc_dsl.q_flags);
						}

						case FieldType::STRING: {
							auto field_value = Serialise::MsgPack(field_spc, obj);
							Xapian::QueryParser queryTexts;
							if (field_spc.flags.bool_term) {
								queryTexts.add_boolean_prefix(fieldname, field_spc.prefix);
							} else {
								queryTexts.add_prefix(fieldname, field_spc.prefix);
							}
							std::string str_texts;
							str_texts.reserve(field_value.length() + field_value.length() + 1);
							str_texts.assign(field_value).append(":").append(field_value);
							return queryTexts.parse_query(str_texts, spc_dsl.q_flags);
						}

						case FieldType::TERM: {
							auto field_value = Serialise::MsgPack(field_spc, obj);
							if (spc_dsl.q_flags & Xapian::QueryParser::FLAG_PARTIAL) {
								Xapian::QueryParser queryString;
								if (field_spc.flags.bool_term) {
									queryString.add_boolean_prefix("_", field_spc.prefix);
								} else {
									queryString.add_prefix("_", field_spc.prefix);
								}
								//queryString.set_database(*database->db);
								auto stopper = getStopper(field_spc.language);
								queryString.set_stopper(stopper.get());
								queryString.set_stemming_strategy(getQueryParserStemStrategy(field_spc.stem_strategy));
								queryString.set_stemmer(Xapian::Stem(field_spc.stem_language));
								std::string str_string;
								str_string.reserve(2 + field_value.length());
								str_string.assign("_:").append(field_value);

								return queryString.parse_query(str_string, spc_dsl.q_flags);
							} else {
								return Xapian::Query(prefixed(field_spc.flags.bool_term ? field_value : lower_string(field_value), field_spc.prefix), spc_dsl.wqf);
							}
						}

						case FieldType::GEO: {
							std::string field_value(Serialise::MsgPack(field_spc, obj));
							// If the region for search is empty, not process this query.
							if (field_value.empty()) {
								return Xapian::Query::MatchNothing;
							}
							return Xapian::Query(prefixed(field_value, field_spc.prefix), spc_dsl.wqf);
						}
						default:
							THROW(QueryDslError, "Type error unexpected %s");
					}
				} catch (const msgpack::type_error&) {
					THROW(QueryDslError, "Type error expected %s in %s", Serialise::type(field_spc.get_type()).c_str(), fieldname.c_str());
				}
			} else {
				THROW(QueryDslError, "Field %s not found in schema", fieldname.c_str());
			}

		}
		break;

		default:
			break;
	}
	return Xapian::Query();
}


bool
QueryDSL::is_reserved(const std::string key)
{
	L_CALL(this, "QueryDSL::is_reserved(%s)", key.c_str());

	return startswith(key, "_");

}


void
QueryDSL::clean_specification()
{
	L_CALL(this, "QueryDSL::clean_specification()");

	specifications.pop_back();
}


void
QueryDSL::set_specifications(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::set_field_specifications()");

	bool found = false;
	specification_dsl spc_dsl;
	try {
		auto const& partial = obj.at(QUERYDSL_PARTIAL);
		if (partial.is_boolean()) {
			if (partial.as_bool())  {
				spc_dsl.q_flags = Xapian::QueryParser::FLAG_PARTIAL;
				found = true;
			}
		} else if (partial.is_number()) {
			if (partial.as_i64()) {
				spc_dsl.q_flags = Xapian::QueryParser::FLAG_PARTIAL;
				found = true;
			}
		} else {
			THROW(QueryDslError, "Type error expected boolean in %s", QUERYDSL_PARTIAL);
		}
	} catch(const std::out_of_range&) { }

	try {
		auto const& boost = obj.at(QUERYDSL_BOOST);
		if (boost.is_number() && boost.getType() != MsgPack::Type::NEGATIVE_INTEGER) {
			spc_dsl.wqf = boost.as_u64();
			found = true;
		} else {
			THROW(QueryDslError, "Type error expected unsigned int in %s", QUERYDSL_BOOST);
		}
	} catch(const std::out_of_range&) { }

	if (found) {
		specifications.push_back(spc_dsl);
	} else {
		if (specifications.empty()) {
			specifications.push_back(specification_dsl());
		} else {
			specifications.push_back(specifications.back());
		}
	}

	/* Add here more specifications */
}


bool
QueryDSL::find_operators(const std::string& key, const MsgPack& obj, Xapian::Query& q)
{
	L_CALL(this, "QueryDSL::find_operators(%s)", repr(key).c_str());

	try {
		auto func = map_op_dispatch_dsl.at(key);
		q = (this->*func)(obj, map_xapian_operator.at(key));
		return true;
	} catch (const std::out_of_range&) {
		return false;
	}
}


bool
QueryDSL::find_casts(const std::string& key, const MsgPack& obj, Xapian::Query& q)
{
	L_CALL(this, "QueryDSL::find_casts(%s)", repr(key).c_str());

	try {
		auto func = map_dispatch_cast.at(key);
		q = (this->*func)(obj);
		return true;
	} catch (const std::out_of_range&) {
		return false;
	}
}


bool
QueryDSL::find_values(const std::string& key, const MsgPack& obj, Xapian::Query& q)
{
	L_CALL(this, "QueryDSL::find_values(%s)", repr(key).c_str());

	try {
		auto func = map_dispatch_dsl.at(key);
		q = (this->*func)(obj.at(key));
		return true;
	} catch (const std::out_of_range&) {
		return false;
	}
}


bool
QueryDSL::find_ranges(const std::string& key, const MsgPack& obj, Xapian::Query& q)
{
	L_CALL(this, "QueryDSL::find_ranges(%s)", repr(key).c_str());

	try {
		auto func = map_range_dispatch_dsl.at(key);
		q = (this->*func)(obj.at(key));
		return true;
	} catch (const std::out_of_range&) {
		return false;
	}
}


bool
QueryDSL::find_date(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::find_date(%s)", repr(obj.to_string()).c_str());

	bool isRange = false;

	try {
		obj.at(QUERYDSL_YEAR);
		isRange = true;
	} catch (const std::out_of_range&) { }

	try {
		obj.at(QUERYDSL_MOTH);
		isRange = true;
	} catch (const std::out_of_range&) { }

	try {
		obj.at(QUERYDSL_DAY);
		isRange = true;
	} catch (const std::out_of_range&) { }

	try {
		obj.at(QUERYDSL_TIME);
		isRange = true;
	} catch (const std::out_of_range&) { }

	return isRange;
}


MsgPack
QueryDSL::make_dsl_query(const query_field_t& e)
{
	L_CALL(this, "Query::make_dsl_query()");

	if (e.query.size() == 1) {
		return to_dsl_query(*e.query.begin());
	} else {
		MsgPack finalDSL(MsgPack::Type::MAP);
		for (const auto& query : e.query) {
			finalDSL["_and"].push_back(to_dsl_query(query));
		}
		return finalDSL;
	}
}


MsgPack
QueryDSL::to_dsl_query(std::string query)
{
	L_CALL(this, "Query::to_dsl_query()");

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
					size_t pos = token.lexeme.find(":");
					if (pos != std::string::npos) {
						object[token.lexeme.substr(0, pos)] = token.lexeme.substr(pos + 1);
					} else {
						object["_value"] = token.lexeme;
					}
					stack_msgpack.push_back(object);
				}
					break;
				default:
					break;
			}
		}

		if (stack_msgpack.size() == 1) {
#ifdef	DEBUG_QUERY_TO_DSL
			L_DEBUG(this, "QUERY DSL:\n%s", stack_msgpack.back().to_string(true).c_str());
#endif
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
