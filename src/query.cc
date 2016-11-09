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

#include "query.h"

#include <tuple>                               // for get, tuple
#include <utility>                             // for pair

#include "booleanParser/BooleanParser.h"       // for BooleanTree
#include "booleanParser/LexicalException.h"    // for LexicalException
#include "booleanParser/SyntacticException.h"  // for SyntacticException
#include "booleanParser/Token.h"               // for TokenType, Token, Toke...
#include "database.h"                          // for Database
#include "database_utils.h"                    // for prefixed, query_field_t
#include "exception.h"                         // for ClientError, MSG_Clien...
#include "geo/wkt_parser.h"                    // for ClientError, MSG_Clien...
#include "log.h"                               // for Log, L_SEARCH, L_CALL
#include "multivalue/generate_terms.h"         // for GenerateTerms
#include "multivalue/range.h"                  // for MultipleValueRange
#include "schema.h"                            // for required_spc_t, FieldType
#include "serialise.h"                         // for _float, boolean, date
#include "utils.h"                             // for lower_string


Query::Query(const std::shared_ptr<Schema>& schema_, const std::shared_ptr<Database>& database_)
	: schema(schema_),
	  database(database_) { }


Xapian::Query
Query::get_query(const query_field_t& e, std::vector<std::string>& suggestions)
{
	L_CALL(this, "Query::get_query()");

	auto aux_flags = e.spelling ? Xapian::QueryParser::FLAG_SPELLING_CORRECTION : 0;
	if (e.synonyms) aux_flags |= Xapian::QueryParser::FLAG_SYNONYM;

	L_SEARCH(this, "e.query size: %zu  Spelling: %d Synonyms: %d", e.query.size(), e.spelling, e.synonyms);
	auto q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | aux_flags;
	auto first = true;
	Xapian::Query queryQ;
	for (const auto& query : e.query) {
		if (first) {
			queryQ = make_query(query, suggestions, q_flags);
			first = false;
		} else {
			queryQ = Xapian::Query(Xapian::Query::OP_AND, queryQ, make_query(query, suggestions, q_flags));
		}
	}
	L_SEARCH(this, "e.query: %s", queryQ.get_description().c_str());

	L_SEARCH(this, "e.partial size: %zu", e.partial.size());
	q_flags = Xapian::QueryParser::FLAG_PARTIAL | aux_flags;
	first = true;
	Xapian::Query queryP;
	for (const auto& partial : e.partial) {
		if (first) {
			queryP = make_query(partial, suggestions, q_flags);
			first = false;
		} else {
			queryP = Xapian::Query(Xapian::Query::OP_AND_MAYBE , queryP, make_query(partial, suggestions, q_flags));
		}
	}
	L_SEARCH(this, "e.partial: %s", queryP.get_description().c_str());

	first = true;
	Xapian::Query queryF;
	if (!e.query.empty()) {
		queryF = queryQ;
		first = false;
	}

	if (!e.partial.empty()) {
		if (first) {
			queryF = queryP;
			first = false;
		} else {
			queryF = Xapian::Query(Xapian::Query::OP_AND, queryF, queryP);
		}
	}

	return queryF;
}


Xapian::Query
Query::make_query(const std::string& str_query, std::vector<std::string>& suggestions, int q_flags)
{
	L_CALL(this, "Query::make_query()");

	if (str_query.compare("*") == 0) {
		suggestions.push_back(std::string());
		return Xapian::Query::MatchAll;
	}

	try {
		BooleanTree booltree(str_query);
		std::vector<Xapian::Query> stack_query;

		while (!booltree.empty()) {
			auto token = booltree.front();
			booltree.pop_front();

			switch (token.type) {
				case TokenType::Not: {
					if (stack_query.size() < 1) {
						throw MSG_ClientError("Bad boolean expression");
					} else {
						auto expression = stack_query.back();
						stack_query.pop_back();
						stack_query.push_back(Xapian::Query(Xapian::Query::OP_AND_NOT, Xapian::Query::MatchAll, expression));
					}
					break;
				}

				case TokenType::Or: {
					if (stack_query.size() < 2) {
						throw MSG_ClientError("Bad boolean expression");
					} else {
						auto letf_expression = stack_query.back();
						stack_query.pop_back();
						auto right_expression = stack_query.back();
						stack_query.pop_back();
						stack_query.push_back(Xapian::Query(Xapian::Query::OP_OR, letf_expression, right_expression));
					}
					break;
				}

				case TokenType::And: {
					if (stack_query.size() < 2) {
						throw MSG_ClientError("Bad boolean expression");
					} else {
						auto letf_expression = stack_query.back();
						stack_query.pop_back();
						auto right_expression = stack_query.back();
						stack_query.pop_back();
						stack_query.push_back(Xapian::Query(Xapian::Query::OP_AND, letf_expression, right_expression));
					}
					break;
				}

				case TokenType::Xor:{
					if (stack_query.size() < 2) {
						throw MSG_ClientError("Bad boolean expression");
					} else {
						auto letf_expression = stack_query.back();
						stack_query.pop_back();
						auto right_expression = stack_query.back();
						stack_query.pop_back();
						stack_query.push_back(Xapian::Query(Xapian::Query::OP_XOR, letf_expression, right_expression));
					}
					break;
				}

				case TokenType::Id:
					stack_query.push_back(build_query(token.lexeme, suggestions, q_flags));
					break;

				default:
					break;
			}
		}

		if (stack_query.size() == 1) {
			return stack_query.back();
		} else {
			throw MSG_ClientError("Bad boolean expression");
		}
	} catch (const LexicalException& err) {
		throw MSG_ClientError(err.what());
	} catch (const SyntacticException& err) {
		throw MSG_ClientError(err.what());
	}
}


Xapian::Query
Query::build_query(const std::string& token, std::vector<std::string>& suggestions, int q_flags)
{
	L_CALL(this, "Query::build_query()");

	FieldParser fp(token);
	fp.parse();

	auto field_name_dot = fp.get_field_dot();
	auto field_name = fp.get_field();
	auto field_value = fp.get_value();

	if (field_name.empty()) {
		if (fp.isrange) {
			auto start = fp.start;
			auto end = fp.end;
			// Get type of the range.
			std::tuple<FieldType, std::string, std::string> ser_type = Serialise::get_range_type(start, end);
			const auto& global_spc = Schema::get_data_global(std::get<0>(ser_type));
			return MultipleValueRange::getQuery(global_spc, field_name, start, end);
		} else {
			// Get type of the field_value.
			auto ser_type = Serialise::get_type(field_value);
			const auto& global_spc = Schema::get_data_global(ser_type.first);
			switch (ser_type.first) {
				case FieldType::TEXT: {
					Xapian::QueryParser queryTexts;
					queryTexts.set_database(*database->db);
					queryTexts.set_stemming_strategy(getQueryParserStrategy(global_spc.stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(global_spc.stem_language));
					suggestions.push_back(queryTexts.get_corrected_query_string());
					return queryTexts.parse_query(field_value, q_flags);
				}
				default:
					return Xapian::Query(prefixed(ser_type.second, global_spc.prefix));
			}
		}
	} else {
		auto data_field = schema->get_data_field(field_name);
		const auto& field_spc = data_field.first;
		if (!data_field.second.empty()) {
			return get_accuracy_query(data_field.second, field_spc.prefix, field_value, fp);
		}

		if (field_spc.flags.inside_namespace) {
			return get_namespace_query(field_name, field_spc.prefix, field_value, fp, q_flags);
		}

		if (fp.isrange) {
			// If this field is not indexed as value, not process this range.
			if (field_spc.slot == default_spc.slot) {
				return Xapian::Query::MatchNothing;
			}
			return MultipleValueRange::getQuery(field_spc, field_name, fp.start, fp.end);
		} else {
			// If the field has not been indexed as a term, not process this term.
			if (field_spc.prefix.empty()) {
				return Xapian::Query::MatchNothing;
			}

			switch (field_spc.get_type()) {
				case FieldType::FLOAT:
					return Xapian::Query(prefixed(Serialise::_float(field_value), field_spc.prefix));
				case FieldType::INTEGER:
					return Xapian::Query(prefixed(Serialise::integer(field_value), field_spc.prefix));
				case FieldType::POSITIVE:
					return Xapian::Query(prefixed(Serialise::positive(field_value), field_spc.prefix));
				case FieldType::STRING:
					if (fp.is_double_quote_value() || q_flags & Xapian::QueryParser::FLAG_PARTIAL) {
						Xapian::QueryParser queryString;
						field_spc.flags.bool_term ? queryString.add_boolean_prefix("_", field_spc.prefix) : queryString.add_prefix("_", field_spc.prefix);

						queryString.set_database(*database->db);
						queryString.set_stemming_strategy(getQueryParserStrategy(field_spc.stem_strategy));
						queryString.set_stemmer(Xapian::Stem(field_spc.stem_language));
						std::string str_string;
						str_string.reserve(2 + field_value.length());
						str_string.assign("_:").append(field_value);

						suggestions.push_back(queryString.get_corrected_query_string());
						return queryString.parse_query(str_string, q_flags);
					} else {
						return Xapian::Query(prefixed(field_spc.flags.bool_term ? field_value : lower_string(field_value), field_spc.prefix));
					}
				case FieldType::TEXT: {
					if (fp.is_double_quote_value()) {
						field_value.assign(fp.get_doubleq_value());
					}
					Xapian::QueryParser queryTexts;
					field_spc.flags.bool_term ? queryTexts.add_boolean_prefix("_", field_spc.prefix) : queryTexts.add_prefix("_", field_spc.prefix);

					queryTexts.set_database(*database->db);
					queryTexts.set_stemming_strategy(getQueryParserStrategy(field_spc.stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(field_spc.stem_language));
					std::string str_texts;
					str_texts.reserve(2 + field_value.length());
					str_texts.assign("_:").append(field_value);

					suggestions.push_back(queryTexts.get_corrected_query_string());
					return queryTexts.parse_query(str_texts, q_flags);
				}
				case FieldType::DATE:
					return Xapian::Query(prefixed(Serialise::date(field_value), field_spc.prefix));
				case FieldType::GEO:
					field_value.assign(Serialise::ewkt(field_value, field_spc.flags.partials, field_spc.error));
					// If the region for search is empty, not process this query.
					if (field_value.empty()) {
						return Xapian::Query::MatchNothing;
					}
					return Xapian::Query(prefixed(field_value, field_spc.prefix));
				case FieldType::UUID:
					return Xapian::Query(prefixed(Serialise::uuid(field_value), field_spc.prefix));
				case FieldType::BOOLEAN:
					return Xapian::Query(prefixed(Serialise::boolean(field_value), field_spc.prefix));
				default:
					break;
			}
		}
	}

	return Xapian::Query::MatchNothing;
}


Xapian::Query
Query::get_accuracy_query(const std::string& field_accuracy, const std::string& prefix_accuracy, const std::string& field_value, const FieldParser& fp)
{
	L_CALL(this, "Query::get_accuracy_query(%s, %s, %s)", repr(field_accuracy).c_str(), repr(prefix_accuracy).c_str(), repr(field_value).c_str());

	if (fp.isrange) {
		throw MSG_ClientError("Accuracy is only indexed like terms, searching by range is not supported");
	}

	try {
		// Check it is date accuracy.
		UnitTime unit = map_acc_date.at(field_accuracy.substr(1));
		static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE));
		Datetime::tm_t tm = Datetime::to_tm_t(field_value);
		switch (unit) {
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
	} catch (const std::out_of_range&) {
		try {
			if (field_accuracy.find("geo") == 0) {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO));
				auto nivel = stox(std::stoull, field_accuracy.substr(4));
				EWKT_Parser ewkt(field_value, DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR);
				auto ranges = ewkt.getRanges();
				return GenerateTerms::geo(ranges, { nivel }, { prefix_type + prefix_accuracy } );
			} else {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER));
				auto acc = stox(std::stoull, field_accuracy.substr(1));
				auto value = stox(std::stoull, field_value);
				return Xapian::Query(prefixed(Serialise::integer(value - GenerateTerms::modulus(value, acc)), prefix_type + prefix_accuracy));
			}
		} catch (const std::invalid_argument& e) {
			throw MSG_ClientError("Invalid numeric value %s: %s [%s]", field_accuracy.c_str(), field_value.c_str(), e.what());
		}
	}
}


Xapian::Query
Query::get_namespace_query(const std::string& full_name, const std::string& prefix_namespace, std::string& field_value, const FieldParser& fp, int q_flags)
{
	L_CALL(this, "Query::get_namespace_query(%s, %s, %s)", repr(full_name).c_str(), repr(prefix_namespace).c_str(), repr(field_value).c_str());

	std::string f_prefix;
	f_prefix.reserve(std::strlen(DOCUMENT_NAMESPACE_TERM_PREFIX) + prefix_namespace.length() + 1);
	f_prefix.assign(DOCUMENT_NAMESPACE_TERM_PREFIX).append(prefix_namespace);
	if (field_value.empty()) {
		return Xapian::Query(DOCUMENT_NAMESPACE_TERM_PREFIX);
	}

	FieldType type;
	if (field_value != "*") {
		auto ser_type = Serialise::get_type(field_value);
		type = ser_type.first;
		f_prefix.append(1, toUType(type));
		field_value.assign(ser_type.second);
	} else {
		type = FieldType::TEXT;
	}

	if (fp.isrange) {
		auto namespace_spc = Schema::get_data_global(type);
		namespace_spc.slot = get_slot(f_prefix);
		return MultipleValueRange::getQuery(namespace_spc, full_name, fp.start, fp.end);
	}

	const auto& namespace_spc = Schema::get_data_global(type);
	switch (namespace_spc.get_type()) {
		case FieldType::TEXT: {
			Xapian::QueryParser queryTexts;
			queryTexts.set_database(*database->db);
			queryTexts.set_stemming_strategy(getQueryParserStrategy(namespace_spc.stem_strategy));
			queryTexts.set_stemmer(Xapian::Stem(namespace_spc.stem_language));
			namespace_spc.flags.bool_term ? queryTexts.add_boolean_prefix("_", f_prefix) : queryTexts.add_prefix("_", f_prefix);
			std::string str_texts;
			str_texts.reserve(2 + field_value.length());
			str_texts.assign("_:").append(field_value);
			return queryTexts.parse_query(str_texts, q_flags);
		}
		default:
			return Xapian::Query(prefixed(field_value, f_prefix));
	}
}
