/*
* Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "xxh64.hpp"

#include "query_dsl.h"

#include "database_utils.h"
#include "exception.h"
#include "multivalue/range.h"


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

/* Reserved DSL words used for dates */
constexpr const char QUERYDSL_YEAR[]      = "_year";
constexpr const char QUERYDSL_MOTH[]      = "_moth";
constexpr const char QUERYDSL_DAY[]       = "_day";
constexpr const char QUERYDSL_TIME[]      = "_time";

/* Reserved DSL words used for ranges */
constexpr const char QUERYDSL_RANGE[]     = "_range";
//constexpr const char QUERYDSL_GEO_POLIGON[] = "_polygon";


static constexpr auto HASH_ALL   = xxh64::hash(QUERYDSL_MATCH_ALL);


const std::unordered_map<std::string, Xapian::Query::op> map_xapian_operator({
	{ QUERYDSL_OR,        Xapian::Query::OP_OR         },
	{ QUERYDSL_AND,       Xapian::Query::OP_AND        },
	{ QUERYDSL_XOR,       Xapian::Query::OP_XOR  	   },
	{ QUERYDSL_NOT,       Xapian::Query::OP_AND_NOT    },
});


const std::unordered_map<std::string, FieldType> map_type({
	{ FLOAT_STR,       FieldType::FLOAT        }, { INTEGER_STR,     FieldType::INTEGER     },
	{ POSITIVE_STR,    FieldType::POSITIVE     }, { STRING_STR,      FieldType::STRING      },
	{ TEXT_STR,        FieldType::TEXT         }, { DATE_STR,        FieldType::DATE        },
	{ GEO_STR,         FieldType::GEO          }, { BOOLEAN_STR,     FieldType::BOOLEAN     },
	{ UUID_STR,        FieldType::UUID         },
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
	{ RESERVED_STRING,       &QueryDSL::query          },
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


/* A domain-specific language (DSL) for query */
QueryDSL::QueryDSL(std::shared_ptr<Schema> schema_)
	: schema(schema_),
      state(QUERY::INIT),
      _wqf(1)
{
	q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD;
}


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::get_query()");

	if (obj.is_map() && obj.is_map() == 1) {
		for (auto const& elem : obj) {
			state =	QUERY::GLOBALQUERY;
			auto key = elem.as_string();
			Xapian::Query qry;
			if (find_operators(key, obj.at(key), qry)) {
				return qry;
			} else if (find_values(key, obj, qry)) {
				return qry;
			} else if (find_casts(key, obj, qry)) {
				return qry;
			} else {
				auto const& o = obj.at(key);
				switch (o.getType()) {
					case MsgPack::Type::ARRAY:
						throw MSG_QueryDslError("Unexpected type %s in %s", MsgPackTypes[static_cast<int>(MsgPack::Type::ARRAY)], key.c_str());
					case MsgPack::Type::MAP:
						return process_query(o, key);
					default: {
						state =	QUERY::QUERY;
						_fieldname = key;
						return query(o);
					}
				}
			}
		}
	} else if (obj.is_string() && xxh64::hash(lower_string(obj.as_string())) == HASH_ALL) {
		return Xapian::Query::MatchAll;
	} else {
		throw MSG_QueryDslError("Type error expected map of size one at root level in query dsl");
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
			if (elem.is_map() && elem.size() == 1) {
				for (const auto& field : elem) {
					state = QUERY::GLOBALQUERY;
					auto key = field.as_string();
					Xapian::Query qry;
					if (find_operators(key, elem.at(key), qry)) {
						final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
					} else if (find_values(key, elem, qry)) {
						final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
					} else if (find_casts(key, elem, qry)) {
						final_query.empty() ?  final_query = qry : final_query = Xapian::Query(op, final_query, qry);
					} else {
						if (!startswith(key, "_")) {
							const auto& o = elem.at(key);
							switch (o.getType()) {
								case MsgPack::Type::ARRAY:
									throw MSG_QueryDslError("Unexpected type array in %s", key.c_str());
								case MsgPack::Type::MAP:
									final_query.empty() ? final_query = process_query(o, key) : final_query = Xapian::Query(op, final_query, process_query(o, key));
									break;
								default:
									state = QUERY::QUERY;
									_fieldname = key;
									final_query.empty() ? final_query = query(o) : final_query = Xapian::Query(op, final_query, query(o));
							}
						} else {
							throw MSG_QueryDslError("Unexpected reserved word %s", key.c_str());
						}
					}
				}
			} else {
				throw MSG_QueryDslError("Expected array of objects with one element");
			}
		}
	} else {
		throw MSG_QueryDslError("Type error expected map in boolean operator");
	}
	return final_query;
}


Xapian::Query
QueryDSL::process_query(const MsgPack& obj, const std::string& field_name)
{
	L_CALL(this, "QueryDSL::process_query(%s)", repr(field_name).c_str());

	_fieldname = field_name;
	state = QUERY::QUERY;
	if (obj.is_map()) {
		find_parameters(obj);
		for (const auto& elem : obj) {
			auto key = elem.as_string();
			Xapian::Query qry;
			if (find_values(key, obj, qry)) {
				return qry;
			} else if (find_casts(key, obj, qry)) {
				return qry;
			}
		}
	}

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
				throw MSG_QueryDslError("Unexpected range type %s", key.c_str());
			}
		}
	} else {
		throw MSG_QueryDslError("Expected object type with one element");
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

		case QUERY::QUERY:
			return MultipleValueRange::getQuery(schema->get_data_field(_fieldname), _fieldname, obj);

		default:
			break;
	}
	return Xapian::Query();
}


Xapian::Query
QueryDSL::query(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::query()");

	switch (state) {
		case QUERY::GLOBALQUERY:
		{
			auto ser_type = Serialise::get_type(obj);
			switch (std::get<0>(ser_type)) {
				case FieldType::TEXT: {
					Xapian::QueryParser queryTexts;
					queryTexts.set_stemming_strategy(getQueryParserStrategy(std::get<2>(ser_type).stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(std::get<2>(ser_type).stem_language));
					return queryTexts.parse_query(obj.as_string(), q_flags);
				}

				default:
					return Xapian::Query(prefixed(std::get<1>(ser_type), std::get<2>(ser_type).prefix));
			}
		}
		break;

		case QUERY::QUERY:
		{
			auto field_spc = schema->get_data_field(_fieldname);
			try {
				switch (field_spc.get_type()) {

					case FieldType::DATE:
						if (find_date(obj)) {
							auto field_spc = schema->get_data_field(_fieldname);
							auto ser_date = Serialise::date(field_spc, obj);
							return Xapian::Query(prefixed(ser_date, field_spc.prefix), _wqf);
						}

					case FieldType::INTEGER:
					case FieldType::POSITIVE:
					case FieldType::FLOAT:
					case FieldType::UUID:
					case FieldType::BOOLEAN:
						return Xapian::Query(prefixed(Serialise::MsgPack(field_spc, obj), field_spc.prefix), _wqf);

					case FieldType::STRING:
					{
						auto field_value = Serialise::MsgPack(field_spc, obj);
						return Xapian::Query(prefixed(field_spc.flags.bool_term ? field_value : lower_string(field_value), field_spc.prefix), _wqf);
					}
					case FieldType::TEXT:
					{
						auto field_value = Serialise::MsgPack(field_spc, obj);
						Xapian::QueryParser queryTexts;
						field_spc.flags.bool_term ? queryTexts.add_boolean_prefix(_fieldname, field_spc.prefix) : queryTexts.add_prefix(_fieldname, field_spc.prefix);
						queryTexts.set_stemming_strategy(getQueryParserStrategy(field_spc.stem_strategy));
						queryTexts.set_stemmer(Xapian::Stem(field_spc.stem_language));
						std::string str_texts;
						str_texts.reserve(field_value.length() + field_value.length() + 1);
						str_texts.assign(field_value).append(":").append(field_value);
						return queryTexts.parse_query(str_texts, q_flags);
					}
					case FieldType::GEO:
					{
						std::string field_value(Serialise::MsgPack(field_spc, obj));
						// If the region for search is empty, not process this query.
						if (field_value.empty()) {
							return Xapian::Query::MatchNothing;
						}
						return Xapian::Query(prefixed(field_value, field_spc.prefix), _wqf);
					}
					default:
						throw MSG_QueryDslError("Type error unexpected %s");
				}
			} catch (const msgpack::type_error&) {
				throw MSG_QueryDslError("Type error expected %s in %s", Serialise::type(field_spc.get_type()).c_str(), _fieldname.c_str());
			}

		}
		break;

		default:
			break;
	}
	return Xapian::Query();
}


void
QueryDSL::find_parameters(const MsgPack& obj)
{
	L_CALL(this, "QueryDSL::find_parameters()");

	try {
		auto const& boost = obj.at(QUERYDSL_BOOST);
		if (boost.is_number() && boost.getType() != MsgPack::Type::NEGATIVE_INTEGER) {
			_wqf = boost.as_u64();
		} else {
			throw MSG_QueryDslError("Type error expected unsigned int in %s", QUERYDSL_BOOST);
		}
	} catch(const std::out_of_range&) { }

	/* Add here more options for the fields */
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
	L(this, "QueryDSL::find_date()");

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
