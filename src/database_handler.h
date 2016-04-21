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

#pragma once

#include "database.h"
#include "manager.h"
#include "multivalue.h"
#include "multivaluekeymaker.h"
#include "schema.h"


extern const std::regex find_types_re;


using SpiesVector = std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>;


class DatabaseHandler {
	const Endpoints* endpoints;
	Schema* schema;
	int flags;
	std::shared_ptr<Database> database;

	void _index(Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length);

	search_t search(const query_field_t& e, std::vector<std::string>& suggestions);
	search_t _search(const std::string& query, std::vector<std::string>& suggestions, int q_flags, const std::string& lan, bool isText=false);

	void get_similar(Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar, bool is_fuzzy=false);
	Xapian::Enquire get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t* e, Multi_MultiValueKeyMaker* sorter, SpiesVector* spies);

public:
	DatabaseHandler();
	~DatabaseHandler();

	inline std::shared_ptr<Database>& get() {
		return database;
	}

	inline Schema* get_schema() {
		return schema;
	}

	inline void checkout() {
		if (!XapiandManager::manager->database_pool.checkout(database, *endpoints, flags)) {
			throw MSG_CheckoutError("Cannot checkout database: %s", endpoints->as_string().c_str());
		}
	}

	inline void checkin() {
		XapiandManager::manager->database_pool.checkin(database);
		database.reset();
	}

	inline void reset(const Endpoints& endpoints_, int flags_) {
		if (endpoints_.size() == 0) {
			throw MSG_ClientError("It is expected at least one endpoint");
		}

		endpoints = &endpoints_;
		flags = flags_;
		schema = new Schema(*XapiandManager::manager->database_pool.get_schema(endpoints->operator[](0), flags));
	}

	Xapian::docid index(const MsgPack& obj, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid index(const std::string& body, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid patch(const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);

	void get_mset(const query_field_t& e, Xapian::MSet& mset, SpiesVector& spies, std::vector<std::string>& suggestions, int offset=0);

	inline Xapian::Document get_document(const Xapian::docid& did) {
		L_CALL(this, "DatabaseHandler::get_document(1)");

		checkout();
		return database->get_document(did);
		checkin();
	}

	Xapian::Document get_document(const std::string& doc_id);
	Xapian::docid get_docid(const std::string& doc_id);

	void delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);

	MsgPack get_value(const Xapian::Document& document, const std::string& slot_name);
	void get_stats_doc(MsgPack& stats, const std::string& document_id);
	void get_stats_database(MsgPack& stats);
};
