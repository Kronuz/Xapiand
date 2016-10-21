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
#include "multivalue/matchspy.h"
#include "multivalue/keymaker.h"
#include "schema.h"


extern const std::regex find_types_re;


using endpoints_error_list = std::unordered_map<std::string, std::vector<std::string>>;


class AggregationMatchSpy;
class Document;


class DatabaseHandler {
	friend class Document;

	Endpoints endpoints;
	int flags;
	HttpMethod method;
	std::shared_ptr<Schema> schema;
	std::shared_ptr<Database> database;

	Document _get_document(const std::string& term_id);
	MsgPack run_script(const MsgPack& data, const std::string& prefix_term_id);

	MsgPack _index(Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length);

	void get_similar(Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar, bool is_fuzzy=false);
	Xapian::Enquire get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t* e, Multi_MultiValueKeyMaker* sorter, AggregationMatchSpy* aggs);

public:
	class lock_database {
		DatabaseHandler* db_handler;
		std::shared_ptr<Database>* database;

		lock_database(const lock_database&) = delete;
		lock_database& operator=(const lock_database&) = delete;

	public:
		lock_database(DatabaseHandler* db_handler_) : db_handler(db_handler_), database(nullptr) {
			lock();
		}

		lock_database(DatabaseHandler& db_handler) : lock_database(&db_handler) { }

		~lock_database() {
			unlock();
		}

		void lock() {
			if (db_handler && !db_handler->database && XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags)) {
				database = &db_handler->database;
			}
		}

		void unlock() noexcept {
			if (database) {
				if (*database) {
					XapiandManager::manager->database_pool.checkin(*database);
				}
				(*database).reset();
				database = nullptr;
			}
		}
	};

	DatabaseHandler();
	DatabaseHandler(const Endpoints &endpoints_, int flags_=0);
	~DatabaseHandler();

	std::shared_ptr<Database> get_database() const noexcept {
		return database;
	}

	std::shared_ptr<Schema> get_schema() const {
		return std::make_shared<Schema>(XapiandManager::manager->database_pool.get_schema(endpoints[0], flags));
	}

	std::shared_ptr<Schema> get_fvschema() const {
		std::shared_ptr<const MsgPack> fvs, fvs_aux;
		for (const auto& e : endpoints) {
			fvs_aux = XapiandManager::manager->database_pool.get_schema(e, flags);	/* Get the first valid schema */
			if (fvs_aux->is_null()) {
				continue;
			}
			if (fvs == nullptr) {
				fvs = fvs_aux;
			} else if (*fvs != *fvs_aux) {
				throw MSG_ClientError("Cannot index in several indexes with different schemas");
			}
		}
		return std::make_shared<Schema>(fvs ? fvs : fvs_aux);
	}

	void reset(const Endpoints& endpoints_, int flags_, HttpMethod method_);

	Xapian::docid index(const std::string& body, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length, endpoints_error_list* err_list=nullptr);
	Xapian::docid index(const MsgPack& obj, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	Xapian::docid patch(const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length);
	void write_schema(const std::string& body);
	void write_schema(const MsgPack& obj);

	void get_mset(const query_field_t& e, Xapian::MSet& mset, AggregationMatchSpy* aggs, const MsgPack* qdsl, std::vector<std::string>& suggestions, int offset=0);

	void update_schema() const;
	void update_schemas() const;

	Document get_document(const Xapian::docid& did);
	Document get_document(const std::string& doc_id);
	Xapian::docid get_docid(const std::string& doc_id);

	void delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);
	endpoints_error_list multi_db_delete_document(const std::string& doc_id, bool commit_=false, bool wal_=true);

	void get_document_info(MsgPack& info, const std::string& document_id);
	void get_database_info(MsgPack& info);
};


std::string join_data(const std::string& obj, const std::string& blob);
std::string split_data_obj(const std::string& data);
std::string split_data_blob(const std::string& data);


class Document {
	friend class DatabaseHandler;

	mutable Xapian::Document _document;
	mutable std::shared_ptr<Database> _database;

	DatabaseHandler* db_handler;

	Xapian::Document& doc() {
		L_CALL(this, "Document::doc(%d)", db_handler && db_handler->database && _database != db_handler->database);

		if (db_handler && db_handler->database && _database != db_handler->database) {
			_database = db_handler->database;
			_document = _database->get_document(_document.get_docid());
		}
		return _document;
	}

	const Xapian::Document& doc() const {
		L_CALL(this, "Document::doc(%d) const", db_handler->database && _database != db_handler->database);

		if (db_handler && db_handler->database && _database != db_handler->database) {
			_database = db_handler->database;
			_document = _database->get_document(_document.get_docid());
		}
		return _document;
	}

public:
	Document()
		: db_handler(nullptr) { }

	Document(const Xapian::Document &doc)
		: _document(doc),
		  db_handler(nullptr) { }

	Document(DatabaseHandler* db_handler_, const Xapian::Document &doc)
		: _document(doc),
		  _database(db_handler_->database),
		  db_handler(db_handler_) { }

	std::string get_value(Xapian::valueno slot) const {
		L_CALL(this, "Document::get_value()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().get_value(slot);
	}

	MsgPack get_value(const std::string& slot_name) const {
		L_CALL(this, "Document::get_value(%s)", slot_name.c_str());

		auto schema = db_handler->get_schema();
		auto slot_field = schema->get_slot_field(slot_name);

		return Unserialise::MsgPack(slot_field.get_type(), get_value(slot_field.slot));
	}

	void add_value(Xapian::valueno slot, const std::string& value) {
		L_CALL(this, "Document::add_value()");

		_document.add_value(slot, value);
	}

	void add_value(const std::string& slot_name, const MsgPack& value) {
		L_CALL(this, "Document::add_value(%s)", slot_name.c_str());

		auto schema = db_handler->get_schema();
		auto slot_field = schema->get_slot_field(slot_name);

		add_value(slot_field.slot, Serialise::MsgPack(slot_field, value));
	}

	void remove_value(Xapian::valueno slot) {
		L_CALL(this, "Document::remove_value()");

		_document.remove_value(slot);
	}

	void remove_value(const std::string& slot_name) {
		L_CALL(this, "Document::remove_value(%s)", slot_name.c_str());

		auto schema = db_handler->get_schema();
		auto slot_field = schema->get_slot_field(slot_name);

		remove_value(slot_field.slot);
	}

	void clear_values() {
		L_CALL(this, "Document::clear_values()");

		_document.clear_values();
	}

	std::string get_data() const {
		L_CALL(this, "Document::get_data()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().get_data();
	}

	void set_data(const std::string& data) {
		L_CALL(this, "Document::set_data(%s)", repr(data).c_str());

		_document.set_data(data);
	}

	void set_data(const std::string& obj, const std::string& blob) {
		L_CALL(this, "Document::set_data(...)");

		set_data(::join_data(obj, blob));
	}

	std::string get_blob() {
		L_CALL(this, "Document::get_blob()");

		return ::split_data_blob(get_data());
	}

	void set_blob(const std::string& blob) {
		L_CALL(this, "Document::set_blob()");

		DatabaseHandler::lock_database lk(db_handler);  // optimize nested database locking
		set_data(::split_data_obj(get_data()), blob);
	}

	MsgPack get_obj() const {
		L_CALL(this, "Document::get_obj()");

		return MsgPack::unserialise(::split_data_obj(get_data()));
	}

	void set_obj(const MsgPack& obj) {
		L_CALL(this, "Document::get_obj()");

		DatabaseHandler::lock_database lk(db_handler);  // optimize nested database locking
		set_data(obj.serialise(), get_blob());
	}

	void add_posting(const std::string& tname, Xapian::termpos tpos, Xapian::termcount wdfinc = 1) {
		L_CALL(this, "Document::add_posting()");

		_document.add_posting(tname, tpos, wdfinc);
	}

	void add_term(const std::string& tname, Xapian::termcount wdfinc = 1) {
		L_CALL(this, "Document::add_term()");

		_document.add_term(tname, wdfinc);
	}

	void add_boolean_term(const std::string& term) {
		L_CALL(this, "Document::add_boolean_term()");

		add_term(term, 0);
	}

	void remove_posting(const std::string& tname, Xapian::termpos tpos, Xapian::termcount wdfdec = 1) {
		L_CALL(this, "Document::remove_posting()");

		_document.remove_posting(tname, tpos, wdfdec);
	}

	void remove_term(const std::string& tname) {
		L_CALL(this, "Document::remove_term()");

		_document.remove_term(tname);
	}

	void clear_terms() {
		L_CALL(this, "Document::clear_terms()");

		_document.clear_terms();
	}

	Xapian::termcount termlist_count() const {
		L_CALL(this, "Document::termlist_count()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().termlist_count();
	}

	Xapian::TermIterator termlist_begin() const {
		L_CALL(this, "Document::termlist_begin()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().termlist_begin();
	}

	Xapian::TermIterator termlist_end() const noexcept {
		L_CALL(this, "Document::termlist_end()");

	    return Xapian::TermIterator();
	}

	Xapian::termcount values_count() const {
		L_CALL(this, "Document::values_count()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().values_count();
	}

	Xapian::ValueIterator values_begin() const {
		L_CALL(this, "Document::values_begin()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().values_begin();
	}

	Xapian::ValueIterator values_end() const noexcept {
		L_CALL(this, "Document::values_end()");

	    return Xapian::ValueIterator();
	}

	Xapian::docid get_docid() const {
		L_CALL(this, "Document::get_docid()");

		return _document.get_docid();
	}

	std::string serialise() const {
		L_CALL(this, "Document::serialise()");

		DatabaseHandler::lock_database lk(db_handler);
		return doc().serialise();
	}

	static Document unserialise(DatabaseHandler* db_handler, const std::string& serialised) {
		L_CALL(nullptr, "Document::unserialise()");

		return Document(db_handler, Xapian::Document::unserialise(serialised));
	}

	static Document unserialise(const std::string& serialised) {
		return unserialise(nullptr, serialised);
	}
};
