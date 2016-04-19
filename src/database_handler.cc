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

#include "database_utils.h"
#include "datetime.h"
#include "generate_terms.h"
#include "msgpack_patcher.h"
#include "multivaluerange.h"
#include "serialise.h"
#include "wkt_parser.h"


static const std::regex find_field_re("(([_a-z][_a-z0-9]*):)?(\"[^\"]+\"|[^\": ]+)[ ]*", std::regex::icase | std::regex::optimize);


DatabaseHandler::DatabaseHandler()
	: endpoints(nullptr),
	  schema(nullptr),
	  flags(0) { }


DatabaseHandler::~DatabaseHandler() {
	if (database) {
		checkin();
	}
}


void
DatabaseHandler::_index(Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(this, "DatabaseHandler::_index()");

	auto properties = schema->getPropertiesSchema();
	specification_t specification;

	// Index Required Data.
	term_id = schema->serialise_id(properties, specification, _document_id);

	std::size_t found = ct_type.find_last_of("/");
	std::string type(ct_type.c_str(), found);
	std::string subtype(ct_type.c_str(), found + 1, ct_type.size());

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
	std::string term_prefix = get_prefix("content_type", DOCUMENT_CUSTOM_TERM_PREFIX, STRING_TYPE);
	doc.add_term(prefixed(ct_type, term_prefix));
	doc.add_term(prefixed(type + "/*", term_prefix));
	doc.add_term(prefixed("*/" + subtype, term_prefix));

	// Index obj.
	// Save a copy of schema for undo changes if there is a exception.
	auto str_schema = schema->to_string();
	auto _to_store = schema->get_store();

	try {
		Schema::data_t schema_data(doc);
		TaskVector tasks;
		tasks.reserve(obj.size());
		for (const auto item_key : obj) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_reserved.at(str_key);
				(schema->*func)(properties, obj.at(str_key), schema_data.specification);
			} catch (const std::out_of_range&) {
				if (is_valid(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, schema, std::ref(properties), obj.at(str_key), std::ref(schema_data), std::move(str_key)));
				} else {
					try {
						auto func = map_dispatch_root.at(str_key);
						tasks.push_back(std::async(std::launch::deferred, func, schema, std::ref(properties), obj.at(str_key), std::ref(schema_data)));
					} catch (const std::out_of_range&) { }
				}
			}
		}

		schema->restart_specification(schema_data.specification);
		const specification_t spc_start = schema_data.specification;
		for (auto& task : tasks) {
			task.get();
			schema_data.specification = spc_start;
		}

		for (const auto& elem : schema_data.map_values) {
			doc.add_value(elem.first, elem.second.serialise());
		}
	} catch (...) {
		// Back to the initial schema if there are changes.
		if (schema->get_store()) {
			schema->set_schema(str_schema);
			schema->set_store(_to_store);
		}
		throw;
	}
}


Xapian::docid
DatabaseHandler::index(const std::string &body, const std::string &_document_id, bool commit_, const std::string &ct_type, const std::string &ct_length)
{
	L_CALL(this, "DatabaseHandler::index(1)");

	if (!(flags & DB_WRITABLE)) {
		throw MSG_Error("Database is read-only");
	}

	if (_document_id.empty()) {
		throw MSG_Error("Document must have an 'id'");
	}

	// Create MsgPack object
	auto blob = true;
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
				ct_type_ = JSON_TYPE;
			} catch (const std::exception&) { }
			break;
		case MIMEType::APPLICATION_X_MSGPACK:
			obj = MsgPack(body);
			break;
		default:
			break;
	}

	L_INDEX(this, "Document to index: %s", body.c_str());
	Xapian::Document doc;
	std::string term_id;

	if (obj.type() == msgpack::type::MAP) {
		blob = false;
		_index(doc, obj, term_id, _document_id, ct_type_, ct_length);
	}

	set_data(doc, obj.to_string(), blob ? body : "");
	L_INDEX(this, "Schema: %s", schema->to_json_string().c_str());

	checkout();
	auto did = database->replace_document_term(term_id, doc, commit_);
	checkin();

	if (schema->get_store()) {
		schema->set_store(false);
		XapiandManager::manager->database_pool.set_schema(endpoints->operator[](0), flags, std::shared_ptr<const Schema>(schema));
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

	if (obj.type() == msgpack::type::MAP) {
		_index(doc, obj, term_id, _document_id, ct_type, ct_length);
	}

	set_data(doc, obj.to_string(), "");
	L_INDEX(this, "Schema: %s", schema->to_json_string().c_str());

	checkout();
	auto did = database->replace_document_term(term_id, doc, commit_);
	checkin();

	if (schema->get_store()) {
		schema->set_store(false);
		XapiandManager::manager->database_pool.set_schema(endpoints->operator[](0), flags, std::shared_ptr<const Schema>(schema));
	}

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
			_ct_type = JSON_TYPE;
			break;
		case MIMEType::APPLICATION_X_MSGPACK:
			obj_patch = MsgPack(patches);
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

	L_INDEX(this, "Document to index: %s", obj_data.to_json_string().c_str());

	Xapian::Document doc;
	std::string term_id;
	_index(doc, obj_data, term_id, _document_id, _ct_type, ct_length);

	set_data(doc, obj_data.to_string(), get_blob(document));
	L_INDEX(this, "Schema: %s", schema->to_json_string().c_str());

	checkout();
	auto did = database->replace_document_term(term_id, doc, commit_);
	checkin();

	if (schema->get_store()) {
		schema->set_store(false);
		XapiandManager::manager->database_pool.set_schema(endpoints->operator[](0), flags, std::shared_ptr<const Schema>(schema));
	}

	return did;
}


search_t
DatabaseHandler::_search(const std::string& query, std::vector<std::string>& suggestions, int q_flags, const std::string& lan, bool isText)
{
	L_CALL(this, "DatabaseHandler::_search()");

	search_t srch;
	if (query.compare("*") == 0) {
		srch.query = Xapian::Query::MatchAll;
		suggestions.push_back(std::string());
		return srch;
	}

	size_t size_match = 0;
	bool first_time = true, first_timeR = true;
	std::string querystring;
	Xapian::QueryParser queryparser;

	queryparser.set_database(*database->db);

	if (isText) {
		queryparser.set_stemming_strategy(queryparser.STEM_SOME);
		queryparser.set_stemmer(Xapian::Stem(lan.empty() ? default_spc.language[0] : lan));
	}

	// Set for save the prefix added in queryparser.
	std::unordered_set<std::string> added_prefixes;

	std::sregex_iterator next(query.begin(), query.end(), find_field_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		std::string field(next->str(0));
		size_match += next->length(0);
		std::string field_name_dot(next->str(1));
		std::string field_name(next->str(2));
		std::string field_value(next->str(3));
		auto field_t = schema->get_data_field(field_name);

		std::smatch m;
		if (std::regex_match(field_value, m, find_range_re)) {
			// If this field is not indexed as value, not process this query.
			if (field_t.slot == Xapian::BAD_VALUENO) {
				++next;
				continue;
			}

			Xapian::Query queryRange;

			switch (field_t.type) {
				case FLOAT_TYPE:
				case INTEGER_TYPE:
				case POSITIVE_TYPE: {
					auto start(m.str(1)), end(m.str(2));

					queryRange = MultipleValueRange::getQuery(field_t.slot, FLOAT_TYPE, start, end, field_name);

					auto filter_term = GenerateTerms::numeric(start, end, field_t.accuracy, field_t.acc_prefix, added_prefixes, srch.nfps, queryparser);
					if (!filter_term.empty()) {
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, q_flags), queryRange);
					}
					break;
				}
				case STRING_TYPE: {
					auto start(m.str(1)), end(m.str(2));
					queryRange = MultipleValueRange::getQuery(field_t.slot, STRING_TYPE, start, end, field_name);
					break;
				}
				case DATE_TYPE: {
					auto start(m.str(1)), end(m.str(2));

					queryRange = MultipleValueRange::getQuery(field_t.slot, DATE_TYPE, start, end, field_name);

					auto filter_term = GenerateTerms::date(start, end, field_t.accuracy, field_t.acc_prefix, added_prefixes, srch.dfps, queryparser);
					if (!filter_term.empty()) {
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, q_flags), queryRange);
					}
					break;
				}
				case GEO_TYPE: {
					// Validate special case.
					if (field_value.compare("..") == 0) {
						queryRange = Xapian::Query::MatchAll;
						break;
					}

					// The format is: "..EWKT". We always delete double quotes and .. -> EWKT
					field_value.assign(field_value, 3, field_value.size() - 4);

					RangeList ranges;
					CartesianUSet centroids;
					EWKT_Parser::getRanges(field_value, field_t.accuracy[0], field_t.accuracy[1], ranges, centroids);

					queryRange = GeoSpatialRange::getQuery(field_t.slot, ranges, centroids);

					auto filter_term = GenerateTerms::geo(ranges, field_t.accuracy, field_t.acc_prefix, added_prefixes, srch.gfps, queryparser);
					if (!filter_term.empty()) {
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, q_flags), queryRange);
					}
					break;
				}
			}

			// Concatenate with OR all the ranges queries.
			if (first_timeR) {
				srch.query = queryRange;
				first_timeR = false;
			} else {
				srch.query = Xapian::Query(Xapian::Query::OP_OR, srch.query, queryRange);
			}
		} else {
			// If the field has not been indexed as a term, not process this query.
			if (!field_name.empty() && field_t.prefix.empty()) {
				++next;
				continue;
			}

			switch (field_t.type) {
				case FLOAT_TYPE:
				case INTEGER_TYPE:
				case POSITIVE_TYPE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						auto nfp = std::make_unique<NumericFieldProcessor>(field_t.type, field_t.prefix);
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, nfp.get()) : queryparser.add_prefix(field_name, nfp.get());
						srch.nfps.push_back(std::move(nfp));
					}
					field.assign(field_name_dot).append(to_query_string(field_value));
					break;
				case STRING_TYPE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, field_t.prefix) : queryparser.add_prefix(field_name, field_t.prefix);
					}
					break;
				case DATE_TYPE:
					// If there are double quotes, they are deleted: "date" -> date
					if (field_value.at(0) == '"') {
						field_value.assign(field_value, 1, field_value.size() - 2);
					}

					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						auto dfp = std::make_unique<DateFieldProcessor>(field_t.prefix);
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, dfp.get()) : queryparser.add_prefix(field_name, dfp.get());
						srch.dfps.push_back(std::move(dfp));
					}
					field.assign(field_name_dot).append(to_query_string(std::to_string(Datetime::timestamp(field_value))));
					break;
				case GEO_TYPE:
					// Delete double quotes (always): "EWKT" -> EWKT
					field_value.assign(field_value, 1, field_value.size() - 2);
					field_value.assign(Serialise::ewkt(field_value));

					// If the region for search is empty, not process this query.
					if (field_value.empty()) {
						++next;
						continue;
					}

					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, field_t.prefix) : queryparser.add_prefix(field_name, field_t.prefix);
					}
					field.assign(field_name_dot).append(field_value);
					break;
				case BOOLEAN_TYPE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						auto bfp = std::make_unique<BooleanFieldProcessor>(field_t.prefix);
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, bfp.get()) : queryparser.add_prefix(field_name, bfp.get());
						srch.bfps.push_back(std::move(bfp));
					}
					break;
			}

			// Concatenate with OR all the queries.
			if (first_time) {
				querystring = field;
				first_time = false;
			} else {
				querystring += " OR " + field;
			}
		}

		++next;
	}

	if (size_match != query.size()) {
		throw MSG_QueryParserError("Query '" + query + "' contains errors");
	}

	switch (first_time << 1 | first_timeR) {
		case 0:
			try {
				srch.query = Xapian::Query(Xapian::Query::OP_OR, queryparser.parse_query(querystring, q_flags), srch.query);
				suggestions.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError& exc) {
				L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
				throw;
			}
			break;
		case 1:
			try {
				srch.query = queryparser.parse_query(querystring, q_flags);
				suggestions.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError& exc) {
				L_EXC(this, "ERROR: %s", exc.get_msg().c_str());
				throw;
			}
			break;
		case 2:
			suggestions.push_back(std::string());
			break;
		case 3:
			srch.query = Xapian::Query::MatchNothing;
			suggestions.push_back(std::string());
			break;
	}

	return srch;
}


search_t
DatabaseHandler::search(const query_field_t& e, std::vector<std::string>& suggestions)
{
	L_CALL(this, "DatabaseHandler::search()");

	search_t srch_resul;

	auto aux_flags = e.spelling ? Xapian::QueryParser::FLAG_SPELLING_CORRECTION : 0;
	if (e.synonyms) aux_flags |= Xapian::QueryParser::FLAG_SYNONYM;

	L_SEARCH(this, "e.query size: %d  Spelling: %d Synonyms: %d", e.query.size(), e.spelling, e.synonyms);
	auto q_flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_PURE_NOT | aux_flags;
	std::string lan;
	auto first = true;
	auto lit = e.language.begin();
	const auto e_lit = e.language.end();
	Xapian::Query queryQ;
	for (const auto& query : e.query) {
		if (lit != e_lit) {
			lan.assign(*lit++);
		}
		auto srch = _search(query, suggestions, q_flags, lan, true);
		if (first) {
			queryQ = srch.query;
			first = false;
		} else {
			queryQ = Xapian::Query(Xapian::Query::OP_AND, queryQ, srch.query);
		}
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_SEARCH(this, "e.query: %s", queryQ.get_description().c_str());

	L_SEARCH(this, "e.partial size: %d", e.partial.size());
	q_flags = Xapian::QueryParser::FLAG_PARTIAL | aux_flags;
	first = true;
	Xapian::Query queryP;
	for (const auto& partial : e.partial) {
		search_t srch = _search(partial, suggestions, q_flags, lan);
		if (first) {
			queryP = srch.query;
			first = false;
		} else {
			queryP = Xapian::Query(Xapian::Query::OP_AND_MAYBE , queryP, srch.query);
		}
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_SEARCH(this, "e.partial: %s", queryP.get_description().c_str());

	L_SEARCH(this, "e.terms size: %d", e.terms.size());
	q_flags = Xapian::QueryParser::FLAG_BOOLEAN | Xapian::QueryParser::FLAG_PURE_NOT | aux_flags;
	first = true;
	Xapian::Query queryT;
	for (const auto& terms : e.terms) {
		search_t srch = _search(terms, suggestions, q_flags, lan);
		if (first) {
			queryT = srch.query;
			first = false;
		} else {
			queryT = Xapian::Query(Xapian::Query::OP_AND, queryT, srch.query);
		}
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_SEARCH(this, "e.terms: %s", repr(queryT.get_description()).c_str());

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

	if (!e.terms.empty()) {
		if (first) {
			queryF = queryT;
		} else {
			queryF = Xapian::Query(Xapian::Query::OP_AND, queryF, queryT);
		}
	}
	srch_resul.query = queryF;

	return srch_resul;
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
		prefixes.push_back(DOCUMENT_CUSTOM_TERM_PREFIX + Unserialise::type(sim_type));
	}

	for (const auto& sim_field : similar.field) {
		auto field_t = schema->get_data_field(sim_field);
		if (field_t.type != NO_TYPE) {
			prefixes.push_back(field_t.prefix);
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
			auto field_t = schema->get_slot_field(facet);
			if (field_t.type != NO_TYPE) {
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

	// Get the collapse key to use for queries.
	Xapian::valueno collapse_key;
	if (e.collapse.empty()) {
		collapse_key = Xapian::BAD_VALUENO;
	} else {
		auto field_t = schema->get_slot_field(e.collapse);
		collapse_key = field_t.slot;
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
					field.assign(field, 1, field.size() - 1);
					break;
			}
			auto field_t = schema->get_slot_field(field);
			if (field_t.type != NO_TYPE) {
				sorter->add_value(field_t.slot, field_t.type, value, descending);
			}
		}
	}

	checkout();
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			auto srch = search(e, suggestions);
			auto check_at_least = std::min(database->db->get_doccount(), e.check_at_least);
			auto enquire = get_enquire(srch.query, collapse_key, &e, sorter.get(), &spies);
			mset = enquire.get_mset(e.offset + offset, e.limit - offset, check_at_least);
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
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

	auto field_t = schema->get_slot_field(RESERVED_ID);

	Xapian::Query query(prefixed(Serialise::serialise(field_t.type, doc_id), DOCUMENT_ID_TERM_PREFIX));
	Xapian::Document doc;

	checkout();
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			Xapian::Enquire enquire(*database->db);
			enquire.set_query(query);
			auto mset = enquire.get_mset(0, 1);
			if (mset.empty()) {
				throw MSG_DocNotFoundError("Document not found");
			}
			doc = database->db->get_document(*mset.begin());
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			throw MSG_DocNotFoundError("Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			throw MSG_DocNotFoundError("Document not found");
		} catch (const Xapian::Error& exc) {
			throw MSG_Error(exc.get_msg().c_str());
		}
		database->reopen();
	}
	checkin();
	L_DATABASE_WRAP(this, "get_document was done");

	return doc;
}


Xapian::docid
DatabaseHandler::get_docid(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_docid()");

	auto field_t = schema->get_slot_field(RESERVED_ID);

	Xapian::Query query(prefixed(Serialise::serialise(field_t.type, doc_id), DOCUMENT_ID_TERM_PREFIX));
	Xapian::docid _id = 0;

	checkout();
	for (int t = DB_RETRIES; t >= 0; --t) {
		try {
			Xapian::Enquire enquire(*database->db);
			enquire.set_query(query);
			auto mset = enquire.get_mset(0, 1);
			if (mset.empty()) {
				throw MSG_DocNotFoundError("Document not found");
			}
			_id = *mset.begin();
			break;
		} catch (const Xapian::DatabaseModifiedError& exc) {
			if (!t) throw MSG_Error("Database was modified, try again (%s)", exc.get_msg().c_str());
		} catch (const Xapian::NetworkError& exc) {
			if (!t) throw MSG_Error("Problem communicating with the remote database (%s)", exc.get_msg().c_str());
		} catch (const Xapian::InvalidArgumentError&) {
			throw MSG_DocNotFoundError("Document not found");
		} catch (const Xapian::DocNotFoundError&) {
			throw MSG_DocNotFoundError("Document not found");
		} catch (const Xapian::Error& exc) {
			throw MSG_Error(exc.get_msg().c_str());
		}
		database->reopen();
	}
	checkin();
	L_DATABASE_WRAP(this, "get_docid was done");

	return _id;
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


const MsgPack
DatabaseHandler::get_value(const Xapian::Document& document, const std::string& slot_name)
{
	L_CALL(this, "DatabaseHandler::get_value()");

	auto slot_field = schema->get_slot_field(slot_name);

	try {
		return Unserialise::MsgPack(slot_field.type, document.get_value(slot_field.slot));
	} catch (const SerialisationError& exc) {
		throw MSG_Error("Error unserializing value (%s)", exc.get_msg().c_str());
	}

	return MsgPack();
}


const MsgPack
DatabaseHandler::get_stats_doc(const std::string& doc_id)
{
	L_CALL(this, "DatabaseHandler::get_stats_doc()");

	auto doc = get_document(doc_id);

	MsgPack stats;
	stats[RESERVED_ID] = doc.get_value(DB_SLOT_ID);

	MsgPack obj_data = get_MsgPack(doc);
	try {
		obj_data = obj_data.at(RESERVED_DATA);
	} catch (const std::out_of_range&) {
		clean_reserved(obj_data);
	}

	stats[RESERVED_DATA] = std::move(obj_data);

	auto ct_type = doc.get_value(DB_SLOT_TYPE);
	stats["_blob"] = ct_type != JSON_TYPE && ct_type != MSGPACK_TYPE;

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

	return stats;
}


const MsgPack
DatabaseHandler::get_stats_database()
{
	L_CALL(this, "DatabaseHandler::get_stats_database()");

	MsgPack stats;

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

	return stats;
}
