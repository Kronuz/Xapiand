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

#include "database_handler.h"

#include "booleanParser/BooleanParser.h"
#include "booleanParser/SyntacticException.h"
#include "datetime.h"
#include "fields.h"
#include "geo/wkt_parser.h"
#include "msgpack_patcher.h"
#include "multivalue/range.h"
#include "serialise.h"



DatabaseHandler::DatabaseHandler()
	: flags(0) { }


DatabaseHandler::DatabaseHandler(const Endpoints &endpoints_, int flags_)
	: endpoints(endpoints_),
	  flags(flags_)
{
	checkout();
}


DatabaseHandler::~DatabaseHandler() {
	checkin();
}


MsgPack
DatabaseHandler::_index(Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(this, "DatabaseHandler::_index()");

	const auto& properties = schema->getProperties();

	// Index Required Data.
	term_id = schema->serialise_id(properties, _document_id);

	auto found = ct_type.find_last_of("/");
	std::string type(ct_type.c_str(), found);
	std::string subtype(ct_type.c_str(), found + 1, ct_type.length());

	// Saves document's id in DB_SLOT_ID
	doc.add_value(DB_SLOT_ID, term_id);

	// Document's id is also a boolean term (otherwise it doesn't replace an existing document)
	term_id = prefixed(term_id, DOCUMENT_ID_TERM_PREFIX);
	doc.add_boolean_term(term_id);
	L_INDEX(this, "Slot: %d _id: %s (%s)", DB_SLOT_ID, _document_id.c_str(), term_id.c_str());

	// Indexing the content values of data.
	doc.add_value(DB_SLOT_OFFSET, DEFAULT_OFFSET);
	doc.add_value(DB_SLOT_TYPE, ct_type);
	doc.add_value(DB_SLOT_LENGTH, ct_length);

	// Index terms for content-type
	auto term_prefix = get_prefix("content_type", DOCUMENT_CUSTOM_TERM_PREFIX, toUType(FieldType::STRING));
	doc.add_term(prefixed(ct_type, term_prefix));
	doc.add_term(prefixed(type + "/*", term_prefix));
	doc.add_term(prefixed("*/" + subtype, term_prefix));

	// Index obj.
	if (obj.is_map()) {
		return schema->index(properties, obj, doc);
	}

	return obj;
}


Xapian::docid
DatabaseHandler::index(const std::string &body, const std::string &_document_id, bool commit_, const std::string &ct_type, const std::string &ct_length, endpoints_error_list* err_list)
{
	L_CALL(this, "DatabaseHandler::index(1)");

	if (!(flags & DB_WRITABLE)) {
		throw MSG_Error("Database is read-only");
	}

	if (_document_id.empty()) {
		throw MSG_Error("Document must have an 'id'");
	}

	// Create MsgPack object
	auto blob = false;
	auto ct_type_ = ct_type;
	MsgPack obj;
	rapidjson::Document rdoc;
	switch (get_mimetype(ct_type_)) {
		case MIMEType::APPLICATION_JSON:
			json_load(rdoc, body);
			obj = MsgPack(rdoc);
			break;
		case MIMEType::APPLICATION_XWWW_FORM_URLENCODED:
			try {
				json_load(rdoc, body);
				obj = MsgPack(rdoc);
				ct_type_ = JSON_CONTENT_TYPE;
			} catch (const std::exception&) {
				blob = true;
			}
			break;
		case MIMEType::APPLICATION_X_MSGPACK:
			obj = MsgPack::unserialise(body);
			break;
		default:
			blob = true;
			break;
	}

	L_INDEX(this, "Document to index: %s", body.c_str());

	Xapian::Document doc;
	Xapian::docid did;
	std::string term_id;

	if (endpoints.size() == 1) {
		schema = get_schema();

		auto f_data = _index(doc, obj, term_id, _document_id, ct_type_, ct_length);
		L_INDEX(this, "Data: %s", f_data.to_string().c_str());

		set_data(doc, f_data.serialise(), blob ? body : "");
		L_INDEX(this, "Schema: %s", schema->to_string().c_str());

		checkout();
		did = database->replace_document_term(term_id, doc, commit_);
		checkin();
		update_schema();
	} else {
		schema = get_fvschema();

		auto f_data = _index(doc, obj, term_id, _document_id, ct_type_, ct_length);
		L_INDEX(this, "Data: %s", f_data.to_string().c_str());

		set_data(doc, f_data.serialise(), blob ? body : "");
		L_INDEX(this, "Schema: %s", schema->to_string().c_str());

		const auto _endpoints = endpoints;
		for (const auto& e : _endpoints) {
			endpoints.clear();
			endpoints.add(e);
			checkout();
			try {
				did = database->replace_document_term(term_id, doc, commit_);
			} catch (const Xapian::Error& err) {
				err_list->operator[](err.get_error_string()).push_back(e.as_string());
				checkin();
			}
			checkin();
		}
		endpoints = std::move(_endpoints);
		update_schemas();
	}

	return did;
}


Xapian::docid
DatabaseHandler::index(const MsgPack& obj, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(this, "DatabaseHandler::index(2)");

	L_INDEX(this, "Document to index: %s", obj.to_string().c_str());
	Xapian::Document doc;
	std::string term_id;

	schema = std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));

	auto f_data = _index(doc, obj, term_id, _document_id, ct_type, ct_length);
	L_INDEX(this, "Data: %s", f_data.to_string().c_str());

	set_data(doc, f_data.serialise(), "");
	L_INDEX(this, "Schema: %s", schema->to_string().c_str());

	checkout();
	auto did = database->replace_document_term(term_id, doc, commit_);
	checkin();

	update_schema();

	return did;
}


Xapian::docid
DatabaseHandler::patch(const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(this, "DatabaseHandler::patch()");

	if (!(flags & DB_WRITABLE)) {
		throw MSG_Error("database is read-only");
	}

	if (_document_id.empty()) {
		throw MSG_ClientError("Document must have an 'id'");
	}

	rapidjson::Document rdoc_patch;
	auto t = get_mimetype(ct_type);
	MsgPack obj_patch;
	auto _ct_type = ct_type;
	switch (t) {
		case MIMEType::APPLICATION_JSON:
			json_load(rdoc_patch, patches);
			obj_patch = MsgPack(rdoc_patch);
			break;
		case MIMEType::APPLICATION_XWWW_FORM_URLENCODED:
			json_load(rdoc_patch, patches);
			obj_patch = MsgPack(rdoc_patch);
			_ct_type = JSON_CONTENT_TYPE;
			break;
		case MIMEType::APPLICATION_X_MSGPACK:
			obj_patch = MsgPack::unserialise(patches);
			break;
		default:
			throw MSG_ClientError("Patches must be a JSON or MsgPack");
	}

	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(_document_id[0])) {
		prefix.append(":");
	}

	auto document = get_document(_document_id);

	auto obj_data = get_MsgPack(document);

	apply_patch(obj_patch, obj_data);

	L_INDEX(this, "Document to index: %s", obj_data.to_string().c_str());

	Xapian::Document doc;
	std::string term_id;
	auto f_data = _index(doc, obj_data, term_id, _document_id, _ct_type, ct_length);
	L_INDEX(this, "Data: %s", f_data.to_string().c_str());

	set_data(doc, f_data.serialise(), get_blob(document));
	L_INDEX(this, "Schema: %s", schema->to_string().c_str());

	checkout();
	auto did = database->replace_document_term(term_id, doc, commit_);
	checkin();

	update_schema();

	return did;
}


Xapian::Query
DatabaseHandler::_search(const std::string& str_query, std::vector<std::string>& suggestions, int q_flags)
{
	L_CALL(this, "DatabaseHandler::_search()");

	if (str_query.compare("*") == 0) {
		suggestions.push_back(std::string());
		return Xapian::Query::MatchAll;
	}

	try {
		BooleanTree booltree(str_query);
		Xapian::QueryParser queryTerms, queryTexts;

		std::vector<Xapian::Query> stack_query;

		while (!booltree.empty()) {
			Token token = booltree.front();
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
				}
					break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
				case TokenType::Id:
					stack_query.push_back(build_query(token.lexeme, suggestions, q_flags));
					break;

				default:
					// Silence warning from switch
					break;
			}
		}

		if (stack_query.size() != 1) {
			throw MSG_ClientError("Bad boolean expression");
		} else {
			auto query = stack_query.back();
			stack_query.pop_back();
			return query;
		}
	} catch (const LexicalException& err) {
		throw MSG_ClientError(err.what());
	} catch (const SyntacticException& err) {
		throw MSG_ClientError(err.what());
	}
}


Xapian::Query
DatabaseHandler::build_query(std::string token, std::vector<std::string>& suggestions, int q_flags) {

	L_CALL(this, "DatabaseHandler::build_query()");

	std::string str_terms, str_texts;
	Xapian::QueryParser queryTerms, queryTexts;

	// Set for save the prefix added in queryTerms.
	std::unordered_set<std::string> added_prefixes;

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
			std::pair<FieldType, std::string> ser_type;
			if (start.empty() || end.empty()) {
				ser_type = Serialise::get_type(start.empty() ? end : start);
			} else {
				auto ser_type_s = Serialise::get_type(start);
				ser_type = Serialise::get_type(end);
				if (ser_type_s.first != ser_type.first) {
					ser_type.first = FieldType::TEXT;
				}
			}

			const auto& global_spc = Schema::get_data_global(ser_type.first);
			return MultipleValueRange::getQuery(global_spc, field_name, start, end);

		} else {
			// Get type of the field_value.
			auto ser_type = Serialise::get_type(field_value);
			const auto& global_spc = Schema::get_data_global(ser_type.first);
			switch (ser_type.first) {
				default:
					queryTerms.set_database(*database->db);
					str_terms.assign(ser_type.second);

					suggestions.push_back(queryTerms.get_corrected_query_string());
					return queryTerms.parse_query(str_terms, q_flags);

				case FieldType::STRING:
				case FieldType::TEXT:
					queryTexts.set_database(*database->db);
					queryTexts.set_stemming_strategy(getQueryParserStrategy(global_spc.stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(global_spc.language));
					str_texts.assign(field_value);

					suggestions.push_back(queryTexts.get_corrected_query_string());
					return queryTexts.parse_query(str_texts, q_flags);
			}
		}
	} else {
		auto field_spc = schema->get_data_field(field_name);
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
				case FieldType::INTEGER:
				case FieldType::POSITIVE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_spc.prefix).second) {
						auto nfp = new NumericFieldProcessor(field_spc.get_type(), field_spc.prefix);
						field_spc.bool_term ? queryTerms.add_boolean_prefix(field_name, nfp->release()) : queryTerms.add_prefix(field_name, nfp->release());
					}
					to_query_string(field_value);
					queryTerms.set_database(*database->db);
					str_terms.reserve(field_name_dot.length() + field_value.length());
					str_terms.assign(field_name_dot).append(field_value);

					suggestions.push_back(queryTerms.get_corrected_query_string());
					return queryTerms.parse_query(str_terms, q_flags);

				case FieldType::STRING:
					// Xapian does not allow repeat prefixes.
				{
					auto query_str = prefixed(field_spc.bool_term ? field_value : lower_string(field_value), field_spc.prefix);
					return Xapian::Query(query_str);
				}

				case FieldType::TEXT:
					if (fp.off_double_quote_value) {
						field_value = fp.get_doubleq_value();
					}

					if (added_prefixes.insert(field_spc.prefix).second) {
						field_spc.bool_term ? queryTexts.add_boolean_prefix(field_name, field_spc.prefix) : queryTexts.add_prefix(field_name, field_spc.prefix);
					}

					queryTexts.set_database(*database->db);
					queryTexts.set_stemming_strategy(getQueryParserStrategy(field_spc.stem_strategy));
					queryTexts.set_stemmer(Xapian::Stem(field_spc.language));
					str_texts.reserve(field_name_dot.length() + field_value.length());
					str_texts.assign(field_name_dot).append(field_value);

					suggestions.push_back(queryTexts.get_corrected_query_string());
					return queryTexts.parse_query(str_texts, q_flags);

				case FieldType::DATE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_spc.prefix).second) {
						auto dfp = new DateFieldProcessor(field_spc.prefix);
						field_spc.bool_term ? queryTerms.add_boolean_prefix(field_name, dfp->release()) : queryTerms.add_prefix(field_name, dfp->release());
					}

					field_value.assign(std::to_string(Datetime::timestamp(field_value)));
					to_query_string(field_value);

					queryTerms.set_database(*database->db);
					str_terms.reserve(field_name_dot.length() + field_value.length());
					str_terms.assign(field_name_dot).append(field_value);

					suggestions.push_back(queryTerms.get_corrected_query_string());
					return queryTerms.parse_query(str_terms, q_flags);

				case FieldType::GEO:
					field_value.assign(Serialise::ewkt(field_value, field_spc.partials, field_spc.error));

					// If the region for search is empty, not process this query.
					if (field_value.empty()) {
						return Xapian::Query::MatchNothing;
					}

					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_spc.prefix).second) {
						field_spc.bool_term ? queryTerms.add_boolean_prefix(field_name, field_spc.prefix) : queryTerms.add_prefix(field_name, field_spc.prefix);
					}

					queryTerms.set_database(*database->db);
					str_terms.reserve(field_name_dot.length() + field_value.length());
					str_terms.assign(field_name_dot).append(field_value);

					suggestions.push_back(queryTerms.get_corrected_query_string());
					return queryTerms.parse_query(str_terms, q_flags);

				case FieldType::BOOLEAN:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_spc.prefix).second) {
						auto bfp = new BooleanFieldProcessor(field_spc.prefix);
						field_spc.bool_term ? queryTerms.add_boolean_prefix(field_name, bfp->release()) : queryTerms.add_prefix(field_name, bfp->release());
					}

					queryTerms.set_database(*database->db);
					str_terms.reserve(field_name_dot.length() + field_value.length());
					str_terms.assign(field_name_dot).append(field_value);

					suggestions.push_back(queryTerms.get_corrected_query_string());
					return queryTerms.parse_query(str_terms, q_flags);

				default:
					break;
			}
		}
	}

	return Xapian::Query::MatchNothing;
}


Xapian::Query
DatabaseHandler::search(const query_field_t& e, std::vector<std::string>& suggestions)
{
	L_CALL(this, "DatabaseHandler::search()");

	auto aux_flags = e.spelling ? Xapian::QueryParser::FLAG_SPELLING_CORRECTION : 0;
	if (e.synonyms) aux_flags |= Xapian::QueryParser::FLAG_SYNONYM;

	L_SEARCH(this, "e.query size: %zu  Spelling: %d Synonyms: %d", e.query.size(), e.spelling, e.synonyms);
	auto q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | aux_flags;
	auto first = true;
	Xapian::Query queryQ;
	for (const auto& query : e.query) {
		if (first) {
			queryQ = _search(query, suggestions, q_flags);
			first = false;
		} else {
			queryQ = Xapian::Query(Xapian::Query::OP_AND, queryQ, _search(query, suggestions, q_flags));
		}
	}
	L_SEARCH(this, "e.query: %s", queryQ.get_description().c_str());

	L_SEARCH(this, "e.partial size: %zu", e.partial.size());
	q_flags = Xapian::QueryParser::FLAG_PARTIAL | aux_flags;
	first = true;
	Xapian::Query queryP;
	for (const auto& partial : e.partial) {
		if (first) {
			queryP = _search(partial, suggestions, q_flags);
			first = false;
		} else {
			queryP = Xapian::Query(Xapian::Query::OP_AND_MAYBE , queryP, _search(partial, suggestions, q_flags));
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


void
DatabaseHandler::get_similar(Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar, bool is_fuzzy)
{
	L_CALL(this, "DatabaseHandler::get_similar()");

	Xapian::RSet rset;

	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto renquire = get_enquire(query, Xapian::BAD_VALUENO, nullptr, nullptr, nullptr);
			auto mset = renquire.get_mset(0, similar.n_rset);
			for (const auto& doc : mset) {
				rset.add_document(doc);
			}
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			throw MSG_Error(exc.get_msg().c_str());
		}
		database->reopen();
	}

	std::vector<std::string> prefixes;
	prefixes.reserve(similar.type.size() + similar.field.size());
	for (const auto& sim_type : similar.type) {
		std::string prefix(DOCUMENT_CUSTOM_TERM_PREFIX);
		prefix.push_back(toUType(Unserialise::type(sim_type)));
		prefixes.push_back(std::move(prefix));
	}

	for (const auto& sim_field : similar.field) {
		auto field_spc = schema->get_data_field(sim_field);
		if (field_spc.get_type() != FieldType::EMPTY) {
			prefixes.push_back(field_spc.prefix);
		}
	}

	ExpandDeciderFilterPrefixes efp(prefixes);
	auto eset = enquire.get_eset(similar.n_eset, rset, &efp);

	if (is_fuzzy) {
		query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar.n_term));
	} else {
		query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar.n_term);
	}
}


Xapian::Enquire
DatabaseHandler::get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t* e, Multi_MultiValueKeyMaker* sorter, SpiesVector* spies)
{
	L_CALL(this, "DatabaseHandler::get_enquire()");

	Xapian::Enquire enquire(*database->db);

	enquire.set_query(query);

	if (sorter) {
		enquire.set_sort_by_key_then_relevance(sorter, false);
	}

	int collapse_max = 1;
	if (e) {
		if (e->is_nearest) {
			get_similar(enquire, query, e->nearest);
		}

		if (e->is_fuzzy) {
			get_similar(enquire, query, e->fuzzy, true);
		}

		for (const auto& facet : e->facets) {
			auto field_spc = schema->get_slot_field(facet);
			if (field_spc.get_type() != FieldType::EMPTY) {
				auto spy = std::make_unique<MultiValueCountMatchSpy>(get_slot(facet));
				enquire.add_matchspy(spy.get());
				L_SEARCH(this, "added spy -%s-", (facet).c_str());
				spies->push_back(std::make_pair(facet, std::move(spy)));
			}
		}

		collapse_max = e->collapse_max;
	}

	enquire.set_collapse_key(collapse_key, collapse_max);

	return enquire;
}


void
DatabaseHandler::get_mset(const query_field_t& e, Xapian::MSet& mset, SpiesVector& spies, std::vector<std::string>& suggestions, int offset)
{
	L_CALL(this, "DatabaseHandler::get_mset()");

	schema = std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));

	// Get the collapse key to use for queries.
	Xapian::valueno collapse_key;
	if (e.collapse.empty()) {
		collapse_key = Xapian::BAD_VALUENO;
	} else {
		auto field_spc = schema->get_slot_field(e.collapse);
		collapse_key = field_spc.slot;
	}

	std::unique_ptr<Multi_MultiValueKeyMaker> sorter;
	if (!e.sort.empty()) {
		sorter = std::make_unique<Multi_MultiValueKeyMaker>();
		std::string field, value;
		for (const auto& sort : e.sort) {
			size_t pos = sort.find(":");
			if (pos == std::string::npos) {
				field.assign(sort);
				value.clear();
			} else {
				field.assign(sort.substr(0, pos));
				value.assign(sort.substr(pos + 1));
			}
			bool descending = false;
			switch (field.at(0)) {
				case '-':
					descending = true;
				case '+':
					field.erase(field.begin());
					break;
			}
			auto field_spc = schema->get_slot_field(field);
			if (field_spc.get_type() != FieldType::EMPTY) {
				sorter->add_value(field_spc, descending, value, e);
			}
		}
	}

	checkout();
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto query = search(e, suggestions);
			auto check_at_least = std::min(database->db->get_doccount(), e.check_at_least);
			auto enquire = get_enquire(query, collapse_key, &e, sorter.get(), &spies);
			mset = enquire.get_mset(e.offset + offset, e.limit - offset, check_at_least);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const QueryParserError& exc) {
			throw MSG_ClientError("%s", exc.what());
		} catch (const Xapian::QueryParserError& exc) {
			throw MSG_ClientError("%s", exc.get_msg().c_str());
		} catch (const Xapian::Error& exc) {
			throw MSG_Error(exc.get_msg().c_str());
		} catch (const std::exception& exc) {
			throw MSG_ClientError("The search was not performed (%s)", exc.what());
		}
		database->reopen();
	}
	checkin();
}


Xapian::Document
DatabaseHandler::get_document(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_document(2)");

	schema = std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));

	auto field_spc = schema->get_slot_field(RESERVED_ID);

	Xapian::Query query(prefixed(Serialise::MsgPack(field_spc, doc_id), DOCUMENT_ID_TERM_PREFIX));

	checkout();
	Xapian::docid did = database->find_document(query);
	Xapian::Document doc = database->get_document(did);
	checkin();

	return doc;
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_docid()");

	schema = std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));

	auto field_spc = schema->get_slot_field(RESERVED_ID);

	Xapian::Query query(prefixed(Serialise::MsgPack(field_spc, doc_id), DOCUMENT_ID_TERM_PREFIX));

	checkout();
	Xapian::docid did = database->find_document(query);
	checkin();

	return did;
}


void
DatabaseHandler::delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::delete_document(2)");

	auto _id = get_docid(doc_id);

	checkout();
	database->delete_document(_id, commit_, wal_);
	checkin();
}


endpoints_error_list
DatabaseHandler::multi_db_delete_document(const std::string& doc_id, bool commit_, bool wal_)
{
	L_CALL(this, "DatabaseHandler::multi_db_delete_document(2)");

	endpoints_error_list err_list;
	auto _endpoints = endpoints;
	for (const auto& e : _endpoints) {
		endpoints.clear();
		endpoints.add(e);
		try {
			auto _id = get_docid(doc_id);
			checkout();
			database->delete_document(_id, commit_, wal_);
			checkin();
		} catch (const DocNotFoundError& err) {
			err_list["Document not found"].push_back(e.as_string());
			checkin();
		} catch (const Xapian::Error& err) {
			err_list[err.get_error_string()].push_back(e.as_string());
			checkin();
		}
	}
	endpoints = _endpoints;
	return err_list;
}


MsgPack
DatabaseHandler::get_value(const Xapian::Document& document, const std::string& slot_name)
{
	L_CALL(this, "DatabaseHandler::get_value()");

	schema = std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));

	auto slot_field = schema->get_slot_field(slot_name);

	try {
		return Unserialise::MsgPack(slot_field.get_type(), document.get_value(slot_field.slot));
	} catch (const SerialisationError& exc) {
		throw MSG_Error("Error unserializing value (%s)", exc.get_msg().c_str());
	}

	return MsgPack();
}


void
DatabaseHandler::get_stats_doc(MsgPack& stats, const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_stats_doc()");

	auto doc = get_document(doc_id);

	stats[RESERVED_ID] = doc.get_value(DB_SLOT_ID);

	MsgPack obj_data = get_MsgPack(doc);
	try {
		obj_data = obj_data.at(RESERVED_DATA);
	} catch (const std::out_of_range&) { }

	stats[RESERVED_DATA] = std::move(obj_data);

	auto ct_type = doc.get_value(DB_SLOT_TYPE);
	stats["_blob"] = ct_type != JSON_CONTENT_TYPE && ct_type != MSGPACK_CONTENT_TYPE;

	stats["_number_terms"] = doc.termlist_count();

	std::string terms;
	const auto it_e = doc.termlist_end();
	for (auto it = doc.termlist_begin(); it != it_e; ++it) {
		terms += repr(*it) + " ";
	}
	stats[RESERVED_TERMS] = terms;

	stats["_number_values"] = doc.values_count();

	std::string values;
	const auto iv_e = doc.values_end();
	for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
		values += std::to_string(iv.get_valueno()) + ":" + repr(*iv) + " ";
	}
	stats[RESERVED_VALUES] = values;
}


void
DatabaseHandler::get_stats_database(MsgPack& stats)
{
	L_CALL(this, "DatabaseHandler::get_stats_database()");

	checkout();
	unsigned doccount = database->db->get_doccount();
	unsigned lastdocid = database->db->get_lastdocid();
	stats["_uuid"] = database->db->get_uuid();
	stats["_doc_count"] = doccount;
	stats["_last_id"] = lastdocid;
	stats["_doc_del"] = lastdocid - doccount;
	stats["_av_length"] = database->db->get_avlength();
	stats["_doc_len_lower"] =  database->db->get_doclength_lower_bound();
	stats["_doc_len_upper"] = database->db->get_doclength_upper_bound();
	stats["_has_positions"] = database->db->has_positions();
	checkin();
}
