/*
* Copyright (c) 2015-2019 Dubalu LLC
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

#include <strings.h>                              // for strncasecmp
#include <utility>

#include "booleanParser/BooleanParser.h"          // for BooleanTree
#include "booleanParser/LexicalException.h"       // for LexicalException
#include "booleanParser/SyntacticException.h"     // for SyntacticException
#include "cast.h"                                 // for Cast
#include "database_utils.h"                       // for prefixed, RESERVED_VALUE
#include "exception.h"                            // for THROW, QueryDslError
#include "field_parser.h"                         // for FieldParser
#include "hashes.hh"                              // for fnv1ah32
#include "itertools.hh"                           // for iterator::map, iterator::chain
#include "log.h"                                  // for L_CALL, L
#include "modulus.hh"                             // for modulus
#include "multivalue/generate_terms.h"            // for GenerateTerms
#include "multivalue/geospatialrange.h"           // for GeoSpatial, GeoSpatialRange
#include "multivalue/range.h"                     // for MultipleValueRange
#include "repr.hh"                                // for repr
#include "reserved.h"                             // for RESERVED_
#include "serialise.h"                            // for MsgPack, get_range_type...
#include "stopper.h"                              // for getStopper
#include "string.hh"                              // for string::startswith


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #define L_QUERY L_ORANGE


#ifndef L_QUERY
#define L_QUERY L_NOTHING
#endif

constexpr const char RESERVED_QUERYDSL_AND[]                = RESERVED__ "and";
constexpr const char RESERVED_QUERYDSL_OR[]                 = RESERVED__ "or";
constexpr const char RESERVED_QUERYDSL_NOT[]                = RESERVED__ "not";
constexpr const char RESERVED_QUERYDSL_AND_NOT[]            = RESERVED__ "and_not";
constexpr const char RESERVED_QUERYDSL_XOR[]                = RESERVED__ "xor";
constexpr const char RESERVED_QUERYDSL_AND_MAYBE[]          = RESERVED__ "and_maybe";
constexpr const char RESERVED_QUERYDSL_FILTER[]             = RESERVED__ "filter";
constexpr const char RESERVED_QUERYDSL_SCALE_WEIGHT[]       = RESERVED__ "scale_weight";
constexpr const char RESERVED_QUERYDSL_ELITE_SET[]          = RESERVED__ "elite_set";
constexpr const char RESERVED_QUERYDSL_SYNONYM[]            = RESERVED__ "synonym";
constexpr const char RESERVED_QUERYDSL_MAX[]                = RESERVED__ "max";


// A domain-specific language (DSL) for query


QueryDSL::QueryDSL(std::shared_ptr<Schema>  schema_)
	: schema(std::move(schema_)) { }


FieldType
QueryDSL::get_in_type(const MsgPack& obj)
{
	L_CALL("QueryDSL::get_in_type(%s)", repr(obj.to_string()));

	try {
		auto it = obj.find(RESERVED_QUERYDSL_RANGE);
		if (it == obj.end()) {
			// If is not _range must be geo.
			return FieldType::GEO;
		}

		const auto& range = it.value();
		try {
			auto it_f = range.find(RESERVED_QUERYDSL_FROM);
			if (it_f != range.end()) {
				return Serialise::guess_type(it_f.value());
			}
			auto it_t = range.find(RESERVED_QUERYDSL_TO);
			if (it_t != range.end()) {
				return Serialise::guess_type(it_t.value());
			}
			return FieldType::EMPTY;
		} catch (const msgpack::type_error&) {
			THROW(QueryDslError, "%s must be object [%s]", RESERVED_QUERYDSL_RANGE, repr(range.to_string()));
		}
	} catch (const msgpack::type_error&) {
		THROW(QueryDslError, "%s must be object [%s]", RESERVED_QUERYDSL_IN, repr(obj.to_string()));
	}

	return FieldType::EMPTY;
}


std::pair<FieldType, MsgPack>
QueryDSL::parse_guess_range(const required_spc_t& field_spc, std::string_view range)
{
	L_CALL("QueryDSL::parse_guess_range(<field_spc>, %s)", repr(range));

	FieldParser fp(range);
	fp.parse();
	if (!fp.is_range()) {
		THROW(QueryDslError, "Invalid range [<string>]: %s", repr(range));
	}

	MsgPack value;
	auto& _range = value[RESERVED_QUERYDSL_RANGE] = MsgPack(MsgPack::Type::MAP);
	auto start = fp.get_start();
	auto field_type = FieldType::EMPTY;
	if (!start.empty()) {
		auto& obj = _range[RESERVED_QUERYDSL_FROM] = Cast::cast(field_spc.get_type(), start);
		field_type = Serialise::guess_type(obj);
	}
	auto end = fp.get_end();
	if (!end.empty()) {
		auto& obj = _range[RESERVED_QUERYDSL_TO] = Cast::cast(field_spc.get_type(), end);
		if (field_type == FieldType::EMPTY) {
			field_type = Serialise::guess_type(obj);
		}
	}

	return std::make_pair(field_type, std::move(value));
}


MsgPack
QueryDSL::parse_range(const required_spc_t& field_spc, std::string_view range)
{
	L_CALL("QueryDSL::parse_range(<field_spc>, %s)", repr(range));

	FieldParser fp(range);
	fp.parse();
	if (!fp.is_range()) {
		THROW(QueryDslError, "Invalid range [<string>]: %s", repr(range));
	}

	MsgPack value;
	auto& _range = value[RESERVED_QUERYDSL_RANGE] = MsgPack(MsgPack::Type::MAP);
	auto start = fp.get_start();
	if (!start.empty()) {
		_range[RESERVED_QUERYDSL_FROM] = Cast::cast(field_spc.get_type(), start);
	}
	auto end = fp.get_end();
	if (!end.empty()) {
		_range[RESERVED_QUERYDSL_TO] = Cast::cast(field_spc.get_type(), end);
	}

	return value;
}


inline Xapian::Query
QueryDSL::process(Xapian::Query::op op, std::string_view path, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::process(%d, %s, %s, <wqf>, <flags>)", (int)op, repr(path), repr(obj.to_string()));

	Xapian::Query final_query;
	if (op == Xapian::Query::OP_AND_NOT) {
		final_query = Xapian::Query(std::string());
	}

	switch (obj.getType()) {
		case MsgPack::Type::MAP: {
			const auto it_e = obj.end();
			for (auto it = obj.begin(); it != it_e; ++it) {
				const auto field_name = it->str_view();
				if (field_name.empty()) {
					THROW(QueryDslError, "Invalid field name: must not be empty");
				}
				auto const& o = it.value();

				L_QUERY(STEEL_BLUE + "%s = %s" + CLEAR_COLOR, repr(field_name), o.to_string());

				Xapian::Query query;

				if (field_name[0] == reserved__) {
					constexpr static auto _ = phf::make_phf({
						// Compound query clauses
						hh(RESERVED_QUERYDSL_AND),
						hh(RESERVED_QUERYDSL_OR),
						hh(RESERVED_QUERYDSL_NOT),
						hh(RESERVED_QUERYDSL_AND_NOT),
						hh(RESERVED_QUERYDSL_XOR),
						hh(RESERVED_QUERYDSL_AND_MAYBE),
						hh(RESERVED_QUERYDSL_FILTER),
						hh(RESERVED_QUERYDSL_SCALE_WEIGHT),
						hh(RESERVED_QUERYDSL_ELITE_SET),
						hh(RESERVED_QUERYDSL_SYNONYM),
						hh(RESERVED_QUERYDSL_MAX),
						// Leaf query clauses.
						hh(RESERVED_QUERYDSL_IN),
						hh(RESERVED_QUERYDSL_RAW),
						hh(RESERVED_VALUE),
						// Reserved cast words
						hh(RESERVED_FLOAT),
						hh(RESERVED_POSITIVE),
						hh(RESERVED_INTEGER),
						hh(RESERVED_BOOLEAN),
						hh(RESERVED_TERM),  // FIXME: remove legacy term
						hh(RESERVED_KEYWORD),
						hh(RESERVED_STRING),  // FIXME: remove legacy string
						hh(RESERVED_TEXT),
						hh(RESERVED_DATE),
						hh(RESERVED_UUID),
						hh(RESERVED_EWKT),
						hh(RESERVED_POINT),
						hh(RESERVED_POLYGON),
						hh(RESERVED_CIRCLE),
						hh(RESERVED_CHULL),
						hh(RESERVED_MULTIPOINT),
						hh(RESERVED_MULTIPOLYGON),
						hh(RESERVED_MULTICIRCLE),
						hh(RESERVED_MULTICONVEX),
						hh(RESERVED_MULTICHULL),
						hh(RESERVED_GEO_COLLECTION),
						hh(RESERVED_GEO_INTERSECTION),
					});
					switch (_.fhh(field_name)) {
						// Compound query clauses
						case _.fhh(RESERVED_QUERYDSL_AND):
							query = process(Xapian::Query::OP_AND, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_OR):
							query = process(Xapian::Query::OP_OR, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_NOT):
							query = process(Xapian::Query::OP_AND_NOT, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_AND_NOT):
							query = process(Xapian::Query::OP_AND_NOT, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_XOR):
							query = process(Xapian::Query::OP_XOR, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_AND_MAYBE):
							query = process(Xapian::Query::OP_AND_MAYBE, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_FILTER):
							query = process(Xapian::Query::OP_FILTER, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_SCALE_WEIGHT):
							// Xapian::Query(OP_SCALE_WEIGHT, subquery, factor)
							query = process(Xapian::Query::OP_SCALE_WEIGHT, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_ELITE_SET):
							query = process(Xapian::Query::OP_ELITE_SET, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_SYNONYM):
							query = process(Xapian::Query::OP_SYNONYM, path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_MAX):
							query = process(Xapian::Query::OP_MAX, path, o, wqf, flags);
							break;
						// Leaf query clauses.
						case _.fhh(RESERVED_QUERYDSL_IN):
							query = get_in_query(path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_QUERYDSL_RAW):
							query = get_raw_query(path, o, wqf, flags);
							break;
						case _.fhh(RESERVED_VALUE):
							query = get_value_query(path, o, wqf, flags);
							break;
						// Reserved cast words
						case _.fhh(RESERVED_FLOAT):
						case _.fhh(RESERVED_POSITIVE):
						case _.fhh(RESERVED_INTEGER):
						case _.fhh(RESERVED_BOOLEAN):
						case _.fhh(RESERVED_TERM):  // FIXME: remove legacy term
						case _.fhh(RESERVED_KEYWORD):
						case _.fhh(RESERVED_STRING):  // FIXME: remove legacy string
						case _.fhh(RESERVED_TEXT):
						case _.fhh(RESERVED_DATE):
						case _.fhh(RESERVED_UUID):
						case _.fhh(RESERVED_EWKT):
						case _.fhh(RESERVED_POINT):
						case _.fhh(RESERVED_POLYGON):
						case _.fhh(RESERVED_CIRCLE):
						case _.fhh(RESERVED_CHULL):
						case _.fhh(RESERVED_MULTIPOINT):
						case _.fhh(RESERVED_MULTIPOLYGON):
						case _.fhh(RESERVED_MULTICIRCLE):
						case _.fhh(RESERVED_MULTICONVEX):
						case _.fhh(RESERVED_MULTICHULL):
						case _.fhh(RESERVED_GEO_COLLECTION):
						case _.fhh(RESERVED_GEO_INTERSECTION):
							query = get_value_query(path, {{ field_name, o }}, wqf, flags);
							break;
					}
				} else {
					if (path.empty()) {
						query = process(op, field_name, o, wqf, flags);
					} else {
						std::string n_parent;
						n_parent.reserve(path.length() + 1 + field_name.length());
						n_parent.append(path).append(1, DB_OFFSPRING_UNION).append(field_name);
						query = process(op, n_parent, o, wqf, flags);
					}
				}
				final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			}
			break;
		}

		case MsgPack::Type::ARRAY: {
			auto processed = itertools::transform<MsgPack>([&](const MsgPack& o){
				return process(op, path, o, wqf, flags);
			}, obj.begin(), obj.end());

			if (final_query.empty()) {
				final_query = Xapian::Query(op, processed.begin(), processed.end());
			} else {
				auto chained = itertools::chain<MsgPack>(
					&final_query, &final_query + 1,
					processed.begin(), processed.end());
				final_query = Xapian::Query(op, chained.begin(), chained.end());
			}
			break;
		}

		default: {
			auto query = get_value_query(path, obj, wqf, flags);
			final_query = final_query.empty() ? query : Xapian::Query(op, final_query, query);
			break;
		}
	}

	return final_query;
}


inline Xapian::Query
QueryDSL::get_in_query(std::string_view path, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_in_query(%s, %s, <wqf>, <flags>)", repr(path), repr(obj.to_string()));

	if (path.empty()) {
		return get_namespace_in_query(default_spc, obj, wqf, flags);
	}

	auto data_field = schema->get_data_field(path, true);
	const auto& field_spc = data_field.first;

	// if (!data_field.second.empty()) {
	// 	return get_accuracy_in_query(field_spc, data_field.second, obj, wqf);
	// }

	if (field_spc.flags.inside_namespace) {
		return get_namespace_in_query(field_spc, obj, wqf, flags);
	}

	try {
		return get_regular_in_query(field_spc, obj, wqf, flags);
	} catch (const SerialisationError&) {
		return get_namespace_in_query(field_spc, obj, wqf, flags);
	}
}



inline Xapian::Query
QueryDSL::get_raw_query(std::string_view path, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_raw_query(%s, %s, <wqf>, <flags>)", repr(path), repr(obj.to_string()));

	if (path.empty()) {
		if (obj.is_string()) {
			const auto aux = Cast::cast(FieldType::EMPTY, obj.str_view());
			return get_namespace_query(default_spc, aux, wqf, flags);
		}
		return get_namespace_query(default_spc, obj, wqf, flags);
	}
	auto data_field = schema->get_data_field(path, false);
	const auto& field_spc = data_field.first;

	if (!data_field.second.empty()) {
		return get_accuracy_query(field_spc, data_field.second, obj.is_string() ? Cast::cast(field_spc.get_type(), obj.str_view()) : obj, wqf);
	}

	if (field_spc.flags.inside_namespace) {
		return get_namespace_query(field_spc, obj.is_string() ? Cast::cast(field_spc.get_type(), obj.str_view()) : obj, wqf, flags);
	}

	try {
		return get_regular_query(field_spc, obj.is_string() ? Cast::cast(field_spc.get_type(), obj.str_view()) : obj, wqf, flags);
	} catch (const SerialisationError&) {
		return get_namespace_query(field_spc, obj.is_string() ? Cast::cast(FieldType::EMPTY, obj.str_view()) : obj, wqf, flags);
	}
}


inline Xapian::Query
QueryDSL::get_value_query(std::string_view path, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_value_query(%s, %s, <wqf>, <flags>)", repr(path), repr(obj.to_string()));

	if (path.empty()) {
		return get_namespace_query(default_spc, obj, wqf, flags);
	}
	auto data_field = schema->get_data_field(path, false);
	const auto& field_spc = data_field.first;

	if (!data_field.second.empty()) {
		return get_accuracy_query(field_spc, data_field.second, obj, wqf);
	}

	if (field_spc.flags.inside_namespace) {
		return get_namespace_query(field_spc, obj, wqf, flags);
	}

	try {
		return get_regular_query(field_spc, obj, wqf, flags);
	} catch (const SerialisationError&) {
		return get_namespace_query(field_spc, obj, wqf, flags);
	}
}


inline Xapian::Query
QueryDSL::get_acc_date_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_acc_date_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	Datetime::tm_t tm = Datetime::DateParser(obj);
	switch (get_accuracy_date(field_accuracy.substr(1))) {
		case UnitTime::SECOND: {
			Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::MINUTE: {
			Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour, tm.min);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::HOUR: {
			Datetime::tm_t _tm(tm.year, tm.mon, tm.day, tm.hour);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::DAY: {
			Datetime::tm_t _tm(tm.year, tm.mon, tm.day);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::MONTH: {
			Datetime::tm_t _tm(tm.year, tm.mon);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::YEAR: {
			Datetime::tm_t _tm(tm.year);
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::DECADE: {
			Datetime::tm_t _tm(GenerateTerms::year(tm.year, 10));
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::CENTURY: {
			Datetime::tm_t _tm(GenerateTerms::year(tm.year, 100));
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::MILLENNIUM: {
			Datetime::tm_t _tm(GenerateTerms::year(tm.year, 1000));
			return Xapian::Query(prefixed(Serialise::serialise(_tm), field_spc.prefix(), required_spc_t::get_ctype(FieldType::DATE)), wqf);
		}
		case UnitTime::INVALID:
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
	}
}


inline Xapian::Query
QueryDSL::get_acc_time_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_acc_time_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	auto acc = get_accuracy_time(field_accuracy.substr(2));
	if (acc == UnitTime::INVALID) {
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
	}

	int64_t value = Datetime::time_to_double(obj);
	return Xapian::Query(prefixed(Serialise::integer(value - modulus(value, static_cast<uint64_t>(acc))), field_spc.prefix(), required_spc_t::get_ctype(FieldType::INTEGER)), wqf);
}


inline Xapian::Query
QueryDSL::get_acc_timedelta_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_acc_timedelta_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	auto acc = get_accuracy_time(field_accuracy.substr(3));
	if (acc == UnitTime::INVALID) {
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
	}

	int64_t value = Datetime::timedelta_to_double(obj);
	return Xapian::Query(prefixed(Serialise::integer(value - modulus(value, static_cast<uint64_t>(acc))), field_spc.prefix(), required_spc_t::get_ctype(FieldType::INTEGER)), wqf);
}


inline Xapian::Query
QueryDSL::get_acc_num_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_acc_num_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	int errno_save;
	auto acc = strict_stoull(&errno_save, field_accuracy.substr(1));
	if (errno_save != 0) {
		THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
	}
	auto value = Cast::integer(obj);
	return Xapian::Query(prefixed(Serialise::integer(value - modulus(value, acc)), field_spc.prefix(), required_spc_t::get_ctype(FieldType::INTEGER)), wqf);
}


inline Xapian::Query
QueryDSL::get_acc_geo_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_acc_geo_query(<required_spc_t>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	if (string::startswith(field_accuracy, "_geo")) {
		int errno_save;
		auto nivel = strict_stoull(&errno_save, field_accuracy.substr(4));
		if (errno_save != 0) {
			THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
		}
		GeoSpatial geo(obj);
		const auto ranges = geo.getGeometry()->getRanges(default_spc.flags.partials, default_spc.error);
		return GenerateTerms::geo(ranges, { nivel }, { field_spc.prefix() }, wqf);
	}

	THROW(QueryDslError, "Invalid field name: %s", field_accuracy);
}


inline Xapian::Query
QueryDSL::get_accuracy_query(const required_spc_t& field_spc, std::string_view field_accuracy, const MsgPack& obj, Xapian::termcount wqf)
{
	L_CALL("QueryDSL::get_accuracy_query(<field_spc>, %s, %s, <wqf>)", repr(field_accuracy), repr(obj.to_string()));

	switch (field_spc.get_type()) {
		case FieldType::INTEGER:
			return get_acc_num_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::DATE:
			return get_acc_date_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::TIME:
			return get_acc_time_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::TIMEDELTA:
			return get_acc_timedelta_query(field_spc, field_accuracy, obj, wqf);
		case FieldType::GEO:
			return get_acc_geo_query(field_spc, field_accuracy, obj, wqf);
		default:
			THROW(Error, "Type: %s does not handle accuracy terms", Serialise::type(field_spc.get_type()));
	}
}


inline Xapian::Query
QueryDSL::get_namespace_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_namespace_query(<field_spc>, %s, <wqf>, <flags>)", repr(obj.to_string()));

	switch (obj.getType()) {
		case MsgPack::Type::NIL:
			return Xapian::Query(field_spc.prefix());
		case MsgPack::Type::STR: {
			auto val = obj.str_view();
			if (val.empty()) {
				return Xapian::Query(field_spc.prefix());
			}
			if (val == "*") {
				return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix());
			}
			break;
		}
		default:
			break;
	}

	auto ser_type = Serialise::guess_serialise(obj);
	auto spc = Schema::get_namespace_specification(std::get<0>(ser_type), field_spc.prefix());

	return get_term_query(spc, std::get<1>(ser_type), wqf, flags);
}


inline Xapian::Query
QueryDSL::get_regular_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_regular_query(<field_spc>, %s, <wqf>, <flags>)", repr(obj.to_string()));

	switch (obj.getType()) {
		case MsgPack::Type::NIL:
			return Xapian::Query(field_spc.prefix());
		case MsgPack::Type::STR: {
			auto val = obj.str_view();
			if (val.empty()) {
				return Xapian::Query(field_spc.prefix());
			} if (val == "*") {
				return Xapian::Query(Xapian::Query::OP_WILDCARD, field_spc.prefix());
			}
			break;
		}
		default:
			break;
	}

	auto serialised_term = Serialise::MsgPack(field_spc, obj);
	return get_term_query(field_spc, serialised_term, wqf, flags);
}


inline Xapian::Query
QueryDSL::get_term_query(const required_spc_t& field_spc, std::string_view serialised_term, Xapian::termcount wqf, unsigned flags)
{
	L_CALL("QueryDSL::get_term_query(<field_spc>, %s, <wqf>, <flags>)", repr(serialised_term));

	switch (field_spc.get_type()) {
		case FieldType::STRING:
		case FieldType::TEXT: {
			// There cannot be non-keyword fields with bool_term
			// flags |= Xapian::QueryParser::FLAG_PARTIAL;
			Xapian::QueryParser parser;
			if (!field_spc.language.empty()) {
				parser.set_stopper(getStopper(field_spc.language).get());
				// parser.set_stopper_strategy(getQueryParserStopStrategy(field_spc.stop_strategy));
			}
			if (!field_spc.stem_language.empty()) {
				parser.set_stemmer(Xapian::Stem(field_spc.stem_language));
				parser.set_stemming_strategy(getQueryParserStemStrategy(field_spc.stem_strategy));
			}
			return parser.parse_query(std::string(serialised_term), flags, field_spc.prefix() + field_spc.get_ctype());
		}

		case FieldType::KEYWORD: {
			std::string serialised_term_holder;
			if (!field_spc.flags.bool_term) {
				serialised_term_holder = string::lower(serialised_term);
				serialised_term = serialised_term_holder;
			}
			if (string::endswith(serialised_term, '*')) {
				serialised_term.remove_suffix(1);
				return Xapian::Query(Xapian::Query::OP_WILDCARD, prefixed(serialised_term, field_spc.prefix(), field_spc.get_ctype()));
			}
			return Xapian::Query(prefixed(serialised_term, field_spc.prefix(), field_spc.get_ctype()), wqf);
		}

		default:
			return Xapian::Query(prefixed(serialised_term, field_spc.prefix(), field_spc.get_ctype()), wqf);
	}
}


inline Xapian::Query
QueryDSL::get_namespace_in_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount /*wqf*/, unsigned /*flags*/)
{
	L_CALL("QueryDSL::get_namespace_in_query(<field_spc>, %s, <wqf>, <flags>)", repr(obj.to_string()));

	if (obj.is_string()) {
		auto parsed = parse_guess_range(field_spc, obj.str_view());
		if (parsed.first == FieldType::EMPTY) {
			return Xapian::Query(std::string());
		}
		if (field_spc.prefix().empty()) {
			return get_in_query(specification_t::get_global(parsed.first), parsed.second);
		}
		return get_in_query(Schema::get_namespace_specification(parsed.first, field_spc.prefix()), parsed.second);
	}
	auto field_type = get_in_type(obj);
	if (field_type == FieldType::EMPTY) {
		return Xapian::Query(std::string());
	}
	if (field_spc.prefix().empty()) {
		return get_in_query(specification_t::get_global(field_type), obj);
	}
	return get_in_query(Schema::get_namespace_specification(field_type, field_spc.prefix()), obj);
}


inline Xapian::Query
QueryDSL::get_regular_in_query(const required_spc_t& field_spc, const MsgPack& obj, Xapian::termcount /*wqf*/, unsigned /*flags*/)
{
	L_CALL("QueryDSL::get_regular_query(<field_spc>, %s, <wqf>, <flags>)", repr(obj.to_string()));

	if (obj.is_string()) {
		return get_in_query(field_spc, parse_range(field_spc, obj.str_view()));
	}
	return get_in_query(field_spc, obj);
}


inline Xapian::Query
QueryDSL::get_in_query(const required_spc_t& field_spc, const MsgPack& obj)
{
	L_CALL("QueryDSL::get_in_query(<field_spc>, %s)", repr(obj.to_string()));

	if (!obj.is_map() || obj.size() != 1) {
		THROW(QueryDslError, "%s must be an object with a single element [%s]", RESERVED_QUERYDSL_IN, repr(obj.to_string()));
	}

	const auto it = obj.begin();
	const auto field_name = it->str_view();
	if (field_name == RESERVED_QUERYDSL_RANGE) {
		const auto& value = it.value();
		if (!value.is_map()) {
			THROW(QueryDslError, "%s must be object [%s]", repr(field_name), repr(value.to_string()));
		}
		return MultipleValueRange::getQuery(field_spc, value);
	}
	switch (Cast::getHash(field_name)) {
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
			THROW(QueryDslError, "Invalid format %s: %s", RESERVED_QUERYDSL_IN, repr(obj.to_string()));
	}
}


MsgPack
QueryDSL::make_dsl_query(const query_field_t& e)
{
	L_CALL("Query::make_dsl_query(<query_field_t>)");

	MsgPack dsl(MsgPack::Type::MAP);
	if (e.query.empty()) {
		dsl = "*";
	} else if (e.query.size() == 1) {
		dsl = make_dsl_query(*e.query.begin());
	} else {
		for (const auto& query : e.query) {
			dsl["_and"].push_back(make_dsl_query(query));
		}
	}
	return dsl;
}


MsgPack
QueryDSL::make_dsl_query(std::string_view query)
{
	L_CALL("Query::make_dsl_query(%s)", repr(query));

	if (query.empty() || query == "*") {
		return "*";
	}

	TokenType last_op(TokenType::EndOfFile);
	try {
		BooleanTree booltree(query);
		std::vector<MsgPack> stack_msgpack;

		while (!booltree.empty()) {
			const auto& token = booltree.front();

			switch (token.get_type()) {
				case TokenType::Not: {
					if (stack_msgpack.empty()) {
						THROW(QueryDslError, "Bad boolean expression");
					}
					create_exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_NOT);
					last_op = TokenType::Not;
					break;
				}

				case TokenType::Or: {
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					}
					if (last_op == token.get_type()) {
						auto ob = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto ob_it = ob.find(RESERVED_QUERYDSL_OR);
						auto it_end = ob.end();
						if (ob_it != it_end) {
							auto last_op_object = ob_it.value();
							last_op_object.push_back(stack_msgpack.back());
							stack_msgpack.pop_back();
							MsgPack object;
							object[RESERVED_QUERYDSL_OR] = last_op_object;
							stack_msgpack.push_back(std::move(object));
						} else {
							auto ob2 = stack_msgpack.back();
							stack_msgpack.pop_back();
							auto ob2_it = ob2.find(RESERVED_QUERYDSL_OR);
							auto ob2_it_end = ob2.end();
							if (ob2_it != ob2_it_end) {
								auto last_op_object = ob2_it.value();
								last_op_object.push_back(ob);
								MsgPack object;
								object[RESERVED_QUERYDSL_OR] = last_op_object;
								stack_msgpack.push_back(std::move(object));
							} else {
								/* parentheses case Ej: ... a or (b or c) ...
								   parentheses force to create a new expression
								*/
								ASSERT(stack_msgpack.size());
								stack_msgpack.push_back(ob2);
								stack_msgpack.push_back(ob);
								create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_OR);
							}
						}
					} else {
						create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_OR);
					}
					last_op = TokenType::Or;
					break;
				}

				case TokenType::And: {
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					}
					if (last_op == token.get_type()) {
						auto ob = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto ob_it = ob.find(RESERVED_QUERYDSL_AND);
						auto it_end = ob.end();
						if (ob_it != it_end) {
							auto last_op_object = ob_it.value();
							last_op_object.push_back(stack_msgpack.back());
							stack_msgpack.pop_back();
							MsgPack object;
							object[RESERVED_QUERYDSL_AND] = last_op_object;
							stack_msgpack.push_back(std::move(object));
						} else {
							auto ob2 = stack_msgpack.back();
							stack_msgpack.pop_back();
							auto ob2_it = ob2.find(RESERVED_QUERYDSL_AND);
							auto ob2_it_end = ob2.end();
							if (ob2_it != ob2_it_end) {
								auto last_op_object = ob2_it.value();
								last_op_object.push_back(ob);
								MsgPack object;
								object[RESERVED_QUERYDSL_AND] = last_op_object;
								stack_msgpack.push_back(std::move(object));
							} else {
								/* parentheses case Ej: ... a and (b and c) ...
								   parentheses force to create a new expression
								*/
								ASSERT(stack_msgpack.size());
								stack_msgpack.push_back(ob2);
								stack_msgpack.push_back(ob);
								create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_AND);
							}
						}
					} else {
						create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_AND);
					}
					last_op = TokenType::And;
					break;
				}

				case TokenType::Maybe: {
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					}
					if (last_op == token.get_type()) {
						auto ob = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto ob_it = ob.find(RESERVED_QUERYDSL_AND_MAYBE);
						auto it_end = ob.end();
						if (ob_it != it_end) {
							auto last_op_object = ob_it.value();
							last_op_object.push_back(stack_msgpack.back());
							stack_msgpack.pop_back();
							MsgPack object;
							object[RESERVED_QUERYDSL_AND_MAYBE] = last_op_object;
							stack_msgpack.push_back(std::move(object));
						} else {
							auto ob2 = stack_msgpack.back();
							stack_msgpack.pop_back();
							auto ob2_it = ob2.find(RESERVED_QUERYDSL_AND_MAYBE);
							auto ob2_it_end = ob2.end();
							if (ob2_it != ob2_it_end) {
								auto last_op_object = ob2_it.value();
								last_op_object.push_back(ob);
								MsgPack object;
								object[RESERVED_QUERYDSL_AND_MAYBE] = last_op_object;
								stack_msgpack.push_back(std::move(object));
							} else {
								/* parentheses case Ej: ... a maybe (b maybe c) ...
								   parentheses force to create a new expression
								*/
								ASSERT(stack_msgpack.size());
								stack_msgpack.push_back(ob2);
								stack_msgpack.push_back(ob);
								create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_AND_MAYBE);
							}
						}
					} else {
						create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_AND_MAYBE);
					}
					last_op = TokenType::Maybe;
					break;
				}

				case TokenType::Xor: {
					if (stack_msgpack.size() < 2) {
						THROW(QueryDslError, "Bad boolean expression");
					}
					if (last_op == token.get_type()) {
						auto ob = stack_msgpack.back();
						stack_msgpack.pop_back();
						auto ob_it = ob.find(RESERVED_QUERYDSL_XOR);
						auto it_end = ob.end();
						if (ob_it != it_end) {
							auto last_op_object = ob_it.value();
							last_op_object.push_back(stack_msgpack.back());
							stack_msgpack.pop_back();
							MsgPack object;
							object[RESERVED_QUERYDSL_XOR] = last_op_object;
							stack_msgpack.push_back(std::move(object));
						} else {
							auto ob2 = stack_msgpack.back();
							stack_msgpack.pop_back();
							auto ob2_it = ob2.find(RESERVED_QUERYDSL_XOR);
							auto ob2_it_end = ob2.end();
							if (ob2_it != ob2_it_end) {
								auto last_op_object = ob2_it.value();
								last_op_object.push_back(ob);
								MsgPack object;
								object[RESERVED_QUERYDSL_XOR] = last_op_object;
								stack_msgpack.push_back(std::move(object));
							} else {
								/* parentheses case Ej: ... a xor (b xor c) ...
								   parentheses force to create a new expression
								*/
								ASSERT(stack_msgpack.size());
								stack_msgpack.push_back(ob2);
								stack_msgpack.push_back(ob);
								create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_XOR);
							}
						}
					} else {
						create_2exp_op_dsl(stack_msgpack, RESERVED_QUERYDSL_XOR);
					}
					last_op = TokenType::Xor;
					break;
				}

				case TokenType::Id:	{
					FieldParser fp(token.get_lexeme());
					fp.parse();

					MsgPack value;
					if (fp.is_range()) {
						value[RESERVED_QUERYDSL_IN] = fp.get_values();
					} else {
						value = fp.get_value();
					}

					auto field_name = fp.get_field_name();

					MsgPack object;
					if (field_name.empty()) {
						object[RESERVED_QUERYDSL_RAW] = value;
					} else {
						object[field_name][RESERVED_QUERYDSL_RAW] = value;
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
		}
		THROW(QueryDslError, "Bad boolean expression");
	} catch (const LexicalException& err) {
		THROW(QueryDslError, err.what());
	} catch (const SyntacticException& err) {
		THROW(QueryDslError, err.what());
	}
}


void
QueryDSL::create_exp_op_dsl(std::vector<MsgPack>& stack_msgpack, const std::string& operator_dsl)
{
	L_CALL("QueryDSL::create_exp_op_dsl(%s)", repr(operator_dsl));

	MsgPack object = {{ operator_dsl, stack_msgpack.back() }}; // expression.
	stack_msgpack.pop_back();
	stack_msgpack.push_back(std::move(object));
}


void
QueryDSL::create_2exp_op_dsl(std::vector<MsgPack>& stack_msgpack, const std::string& operator_dsl)
{
	L_CALL("QueryDSL::create_2exp_op_dsl(%s)", repr(operator_dsl));

	MsgPack object;
	auto& _op = object[operator_dsl] = { nullptr, stack_msgpack.back() };  // right expression
	stack_msgpack.pop_back();
	_op[0] = stack_msgpack.back();  // left expression
	stack_msgpack.pop_back();
	stack_msgpack.push_back(std::move(object));
}


Xapian::Query
QueryDSL::get_query(const MsgPack& obj)
{
	L_CALL("QueryDSL::get_query(%s)", repr(obj.to_string()));

	Xapian::Query query;

	if (obj.is_string() && obj.str_view() == "*") {
		query = Xapian::Query(std::string());
	} else {
		query = process(Xapian::Query::OP_AND, "", obj, 1, Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD);
	}

	L_QUERY("query = " + STEEL_BLUE + "%s" + CLEAR_COLOR + "\n" + DIM_GREY + "%s" + CLEAR_COLOR, query.get_description(), repr(query.serialise()));
	return query;
}


void
QueryDSL::get_sorter(std::unique_ptr<Multi_MultiValueKeyMaker>& sorter, const MsgPack& obj)
{
	L_CALL("QueryDSL::get_sorter(%s)", repr(obj.to_string()));

	sorter = std::make_unique<Multi_MultiValueKeyMaker>();
	switch (obj.getType()) {
		case MsgPack::Type::MAP: {
			const auto it_e = obj.end();
			for (auto it = obj.begin(); it != it_e; ++it) {
				query_field_t e;
				std::string value;
				bool descending = false;
				const auto field = it->str_view();
				auto const& o = it.value();
				const auto it_val_e = o.end();
				for (auto it_val = o.begin(); it_val != it_val_e; ++it_val) {
					const auto field_key = it_val->str_view();
					auto const& val = it_val.value();
					constexpr static auto _srt = phf::make_phf({
						hh(RESERVED_QUERYDSL_ORDER),
						hh(RESERVED_VALUE),
						hh(RESERVED_QUERYDSL_METRIC),
					});
					switch (_srt.fhh(field_key)) {
						case _srt.fhh(RESERVED_QUERYDSL_ORDER):
							if (!val.is_string()) {
								THROW(QueryDslError, "%s must be string (asc/desc) [%s]", RESERVED_QUERYDSL_ORDER, repr(val.to_string()));
							}
							if (strncasecmp(val.as_str().data(), QUERYDSL_DESC, sizeof(QUERYDSL_DESC)) == 0) {
								descending = true;
							}
							break;
						case _srt.fhh(RESERVED_VALUE):
							value = val.as_str();
							break;
						case _srt.fhh(RESERVED_QUERYDSL_METRIC):
							if (!val.is_string()) {
								THROW(QueryDslError, "%s must be string (%s) [%s]", RESERVED_QUERYDSL_METRIC, "levenshtein, leven, jarowinkler, jarow, sorensendice, sorensen, dice, jaccard, lcsubstr, lcs, lcsubsequence, lcsq, soundex, sound, jaro" ,repr(val.to_string()));
							}
							e.metric = val.as_str();
							break;
						default:
							break;
					}
				}
				const auto field_spc = schema->get_slot_field(field);
				if (field_spc.get_type() != FieldType::EMPTY) {
					sorter->add_value(field_spc, descending, value, e);
				}
			}
		}
		break;
		case MsgPack::Type::STR: {
			auto field = obj.as_str();
			bool descending = false;
			query_field_t e;
			switch (field.at(0)) {
				case '-':
					descending = true;
					/* FALLTHROUGH */
				case '+':
					field.erase(field.begin());
					break;
			}
			const auto field_spc = schema->get_slot_field(field);
			if (field_spc.get_type() != FieldType::EMPTY) {
				sorter->add_value(field_spc, descending, "", e);
			}
		}
		break;
		default:
			THROW(QueryDslError, "Invalid format %s: %s", RESERVED_QUERYDSL_SORT, repr(obj.to_string()));
	}

}
