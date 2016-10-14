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


#include "query_dsl.h"

#include "database_utils.h"
#include "exception.h"
#include "multivalue/range.h"


constexpr const char QUERYDSL_TERM[]  = "_term";
constexpr const char QUERYDSL_VALUE[] = "_value";
constexpr const char QUERYDSL_TYPE[]  = "_type";
constexpr const char QUERYDSL_BOOST[] = "_boost";
constexpr const char QUERYDSL_RANGE[] = "_range";
constexpr const char QUERYDSL_FROM[]  = "_from";
constexpr const char QUERYDSL_TO[]    = "_to";
constexpr const char QUERYDSL_EWKT[]  = "_ewkt";


static constexpr auto or_op = const_hash("_or");
static constexpr auto and_op = const_hash("_and");
static constexpr auto xor_op = const_hash("_xor");
static constexpr auto not_op = const_hash("_not");
static constexpr auto reserved_value = const_hash("_value");


static const std::unordered_map<std::string, FieldType> map_type({
	{ FLOAT_STR,       FieldType::FLOAT        }, { INTEGER_STR,     FieldType::INTEGER     },
	{ POSITIVE_STR,    FieldType::POSITIVE     }, { STRING_STR,      FieldType::STRING      },
	{ TEXT_STR,        FieldType::TEXT         }, { DATE_STR,        FieldType::DATE        },
	{ GEO_STR,         FieldType::GEO          }, { BOOLEAN_STR,     FieldType::BOOLEAN     },
	{ UUID_STR,        FieldType::UUID         }
});


/* A domain-specific language (DSL) for query */

QueryDSL::QueryDSL(std::shared_ptr<Schema> schema_) : schema(schema_)
{
	q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD;
}


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	if (obj.is_map() && obj.is_map() == 1) {
		for (auto const& elem : obj) {
			auto str_key = elem.as_string();
			switch (const_hash(lower_string(str_key).c_str())) {
				case or_op:
					return join_queries(obj.at(str_key), Xapian::Query::OP_OR);

				case and_op:
					return join_queries(obj.at(str_key), Xapian::Query::OP_AND);

				case xor_op:
					return join_queries(obj.at(str_key), Xapian::Query::OP_XOR);

				case not_op:
					return join_queries(obj.at(str_key), Xapian::Query::OP_AND_NOT);
				default: {
					auto const& o = obj.at(str_key);
					switch (o.getType()) {
						case MsgPack::Type::ARRAY:
							throw MSG_QueryDslError("Unexpected type %s in %s", MsgPackTypes[static_cast<int>(MsgPack::Type::ARRAY)], str_key.c_str());
						case MsgPack::Type::MAP:
							return process_query(o, str_key);
							break;
						default: {
							return build_query(o, str_key);
						}
					}

				}
			}
		}
	} else {
		throw MSG_QueryDslError("Type error expected map of size one at root level in query dsl");
	}
	return Xapian::Query();
}


Xapian::Query
QueryDSL::build_query(const MsgPack& o, const std::string& field_name, Xapian::termcount wqf, const std::string& type) {

	auto field_spc = schema->get_data_field(field_name);
	std::string type_s;
	try {
		auto _type = type.empty() ? field_spc.get_type() : map_type.at(type);
		switch (_type) {
			case FieldType::FLOAT:
				type_s = FLOAT_STR;
				if (o.getType() == MsgPack::Type::STR) {
					return Xapian::Query(prefixed(Serialise::_float(o.as_string()), field_spc.prefix), wqf);
				} else {
					return Xapian::Query(prefixed(Serialise::_float(o.as_f64()), field_spc.prefix), wqf);
				}
			case FieldType::INTEGER:
				type_s = INTEGER_STR;
				if (o.getType() == MsgPack::Type::STR) {
					return Xapian::Query(prefixed(Serialise::integer(o.as_string()), field_spc.prefix), wqf);
				} else {
					return Xapian::Query(prefixed(Serialise::integer(o.as_i64()), field_spc.prefix));
				}
			case FieldType::POSITIVE:
				type_s = POSITIVE_STR;
				if (o.getType() == MsgPack::Type::STR) {
					return Xapian::Query(prefixed(Serialise::integer(o.as_string()), field_spc.prefix), wqf);
				} else {
					return Xapian::Query(prefixed(Serialise::integer(o.as_u64()), field_spc.prefix), wqf);
				}
			case FieldType::STRING:
			{
				type_s = STRING_STR;
				auto field_value = o.as_string();
				return Xapian::Query(prefixed(field_spc.bool_term ? field_value : lower_string(field_value), field_spc.prefix), wqf);
			}
			case FieldType::TEXT:
			{
				type_s = TEXT_STR;
				auto field_value = o.as_string();
				Xapian::QueryParser queryTexts;
				field_spc.bool_term ? queryTexts.add_boolean_prefix(field_name, field_spc.prefix) : queryTexts.add_prefix(field_name, field_spc.prefix);
				queryTexts.set_stemming_strategy(getQueryParserStrategy(field_spc.stem_strategy));
				queryTexts.set_stemmer(Xapian::Stem(field_spc.stem_language));
				std::string str_texts;
				str_texts.reserve(field_value.length() + field_value.length() + 1);
				str_texts.assign(field_value).append(":").append(field_value);
				return queryTexts.parse_query(str_texts, q_flags);
			}
			case FieldType::DATE:
				type_s = DATE_STR;
				return Xapian::Query(prefixed(Serialise::date(o.as_string()), field_spc.prefix));
			case FieldType::GEO:
			{
				type_s = GEO_STR;
				std::string field_value(Serialise::ewkt(o.as_string(), field_spc.partials, field_spc.error));
				// If the region for search is empty, not process this query.
				if (field_value.empty()) {
					return Xapian::Query::MatchNothing;
				}
				return Xapian::Query(prefixed(field_value, field_spc.prefix), wqf);
			}
			case FieldType::UUID:
				type_s = UUID_STR;
				return Xapian::Query(prefixed(Serialise::uuid(o.as_string()), field_spc.prefix), wqf);
			case FieldType::BOOLEAN:
				type_s = BOOLEAN_STR;
				if (o.getType() == MsgPack::Type::STR) {
					return Xapian::Query(prefixed(Serialise::boolean(o.as_string()), field_spc.prefix), wqf);
				} else {
					return Xapian::Query(prefixed(Serialise::boolean(o.as_bool()), field_spc.prefix), wqf);
				}
			default:
				throw MSG_QueryDslError("Type error unexpected %s i", type.c_str());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_QueryDslError("Type error expected %s in %s", type_s.c_str(), field_name.c_str());
	}
	return Xapian::Query();
}


Xapian::Query
QueryDSL::join_queries(const MsgPack& sub_obj, Xapian::Query::op op)
{
	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query::MatchAll;
	}
	if (sub_obj.is_array()) {
		for (const auto& elem : sub_obj) {
			if (elem.is_map() && elem.size() == 1) {
				for (const auto& field : elem) {
					auto str_ob = field.as_string();
					switch (const_hash(lower_string(str_ob).c_str())) {
						case or_op:
						{
							const auto& o = elem.at(str_ob);
							final_query.empty() ? final_query = join_queries(o, Xapian::Query::OP_OR) : final_query = Xapian::Query(op, final_query, join_queries(o, Xapian::Query::OP_OR));
						}
						break;

						case and_op:
						{
							const auto& o = elem.at(str_ob);
							final_query.empty() ? final_query = join_queries(o, Xapian::Query::OP_AND) : final_query = Xapian::Query(op, final_query, join_queries(o, Xapian::Query::OP_AND));
						}
						break;

						case xor_op:
						{
							const auto& o = elem.at(str_ob);
							final_query.empty() ? final_query = join_queries(o, Xapian::Query::OP_XOR) : final_query = Xapian::Query(op, final_query, join_queries(o, Xapian::Query::OP_XOR));
						}
						break;

						case not_op:
						{
							const auto& o = elem.at(str_ob);
							final_query.empty() ? final_query = join_queries(o, Xapian::Query::OP_AND_NOT) :  final_query = Xapian::Query(op, final_query, join_queries(o, Xapian::Query::OP_AND_NOT));
						}
						break;

						case reserved_value:
							//Empty field name
							break;

						default:
							if (!startswith(str_ob, "_")) {
								const auto& o = elem.at(str_ob);
								switch (o.getType()) {
									case MsgPack::Type::ARRAY:
										throw MSG_QueryDslError("Unexpected type array in %s", str_ob.c_str());

									case MsgPack::Type::MAP:
										final_query.empty() ? final_query = process_query(o, str_ob) : final_query = Xapian::Query(op, final_query, process_query(o, str_ob));
									break;

									default:
										final_query.empty() ? final_query = build_query(o, str_ob) : final_query = Xapian::Query(op, final_query, build_query(o, str_ob));
								}
							} else {
								throw MSG_QueryDslError("Unexpected reserved word %s", str_ob.c_str());
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
QueryDSL::process_query(const MsgPack& obj, const std::string& field_name) {

	uint64_t boost = 1;	/* Default value in xapian */

	if (obj.is_map() && obj.find(QUERYDSL_RANGE) != obj.end()) {
		const MsgPack* to;
		const MsgPack* from;
		const MsgPack& o = obj.at(QUERYDSL_RANGE);
		try {
			to = &o.at(QUERYDSL_TO);
		} catch (const std::out_of_range&) { }

		try {
			from = &o.at(QUERYDSL_FROM);
		} catch (const std::out_of_range&) { }

		if (to || from) {
			return MultipleValueRange::getQuery(schema->get_data_field(field_name), field_name, from, to);
		} else {
			throw MSG_QueryDslError("Expected %s and/or %s in %s", QUERYDSL_FROM, QUERYDSL_TO, QUERYDSL_RANGE);
		}
	} else {
		try {
			/* Get _boost if exist */
			auto const& o_boost = obj.at(QUERYDSL_BOOST);
			if (o_boost.is_number() && o_boost.getType() != MsgPack::Type::NEGATIVE_INTEGER) {
				boost = o_boost.as_u64();
			} else {
				throw MSG_QueryDslError("Type error expected unsigned int in %s", QUERYDSL_BOOST);
			}
		} catch (const std::out_of_range&) { }

		try {
			auto const& val = obj.at(QUERYDSL_VALUE);
			return build_query(val, field_name, boost);
		} catch (const std::out_of_range&) {
			try {
				std::string type;
				auto const& val = obj.at(QUERYDSL_TERM); /* Force to term unused at the moment */
				try {
					/* Get _type if exist */
					auto const& o_type = obj.at(QUERYDSL_TYPE);
					if (o_type.getType() == MsgPack::Type::STR) {
						type = o_type.as_string();
					} else {
						throw MSG_QueryDslError("Type error expected string in %s", QUERYDSL_TYPE);
					}
				} catch (const std::out_of_range&) { }
				return build_query(val, field_name, boost, type);
			} catch (const std::out_of_range&) {
				throw MSG_QueryDslError("Expected %s or %s in object", QUERYDSL_VALUE, QUERYDSL_TERM);
			}
		}
	}
	return Xapian::Query();
}
