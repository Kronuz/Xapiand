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

#include "database.h"

#include "database_autocommit.h"
#include "multivaluerange.h"
#include "log.h"
#include "generate_terms.h"
#include "msgpack_patcher.h"

#include <assert.h>
#include <bitset>

#define XAPIAN_LOCAL_DB_FALLBACK 1

#define DATABASE_UPDATE_TIME 10

#define DEFAULT_OFFSET "0" /* Replace for the real offset */


static const std::regex find_field_re("(([_a-z][_a-z0-9]*):)?(\"[^\"]+\"|[^\": ]+)[ ]*", std::regex::icase | std::regex::optimize);


static auto getPos = [](size_t pos, size_t size) noexcept {
	return pos < size ? pos : size - 1;
};


Database::Database(std::shared_ptr<DatabaseQueue>& queue_, const Endpoints& endpoints_, int flags_)
	: weak_queue(queue_),
	  endpoints(endpoints_),
	  flags(flags_),
	  hash(endpoints.hash()),
	  access_time(system_clock::now()),
	  mastery_level(-1)
{
	reopen();

	if (auto queue = weak_queue.lock()) {
		queue->inc_count();
	}
}


Database::~Database()
{
	if (auto queue = weak_queue.lock()) {
		queue->dec_count();
	}
}


long long
Database::read_mastery(const std::string& dir)
{
	if (!local) return -1;
	if (mastery_level != -1) return mastery_level;

	mastery_level = ::read_mastery(dir, true);

	return mastery_level;
}


void
Database::reopen()
{
	access_time = system_clock::now();

	if (db) {
		// Try to reopen
		try {
			db->reopen();
			schema.setDatabase(this);
			return;
		} catch (const Xapian::Error& err) {
			L_ERR(this, "ERROR: %s", err.get_msg().c_str());
			db->close();
			db.reset();
		}
	}

	Xapian::Database rdb;
	Xapian::WritableDatabase wdb;
	Xapian::Database ldb;

	auto endpoints_size = endpoints.size();

	const Endpoint *e;
	auto i = endpoints.begin();
	if (flags & DB_WRITABLE) {
		db = std::make_unique<Xapian::WritableDatabase>();
		if (endpoints_size != 1) {
			L_ERR(this, "ERROR: Expecting exactly one database, %d requested: %s", endpoints_size, endpoints.as_string().c_str());
		} else {
			e = &*i;
			if (e->is_local()) {
				local = true;
				wdb = Xapian::WritableDatabase(e->path, (flags & DB_SPAWN) ? Xapian::DB_CREATE_OR_OPEN : Xapian::DB_OPEN);
				if (endpoints_size == 1) read_mastery(e->path);
			}
#ifdef HAVE_REMOTE_PROTOCOL
			else {
				local = false;
				// Writable remote databases do not have a local fallback
				int port = (e->port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : e->port;
				wdb = Xapian::Remote::open_writable(e->host, port, 0, 10000, e->path);
			}
#endif
			db->add_database(wdb);
		}
	} else {
		for (db = std::make_unique<Xapian::Database>(); i != endpoints.end(); ++i) {
			e = &*i;
			if (e->is_local()) {
				local = true;
				try {
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (endpoints_size == 1) read_mastery(e->path);
				} catch (const Xapian::DatabaseOpeningError& err) {
					if (!(flags & DB_SPAWN))  {
						db.reset();
						throw;
					}
					wdb = Xapian::WritableDatabase(e->path, Xapian::DB_CREATE_OR_OPEN);
					rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (endpoints_size == 1) read_mastery(e->path);
				}
			}
#ifdef HAVE_REMOTE_PROTOCOL
			else {
				local = false;
# ifdef XAPIAN_LOCAL_DB_FALLBACK
				int port = (e->port == XAPIAND_BINARY_SERVERPORT) ? XAPIAND_BINARY_PROXY : e->port;
				rdb = Xapian::Remote::open(e->host, port, 0, 10000, e->path);
				try {
					ldb = Xapian::Database(e->path, Xapian::DB_OPEN);
					if (ldb.get_uuid() == rdb.get_uuid()) {
						L_DATABASE(this, "Endpoint %s fallback to local database!", e->as_string().c_str());
						// Handle remote endpoints and figure out if the endpoint is a local database
						rdb = Xapian::Database(e->path, Xapian::DB_OPEN);
						local = true;
						if (endpoints_size == 1) read_mastery(e->path);
					}
				} catch (const Xapian::DatabaseOpeningError& err) { }
# else
				rdb = Xapian::Remote::open(e->host, port, 0, 10000, e->path);
# endif
			}
#endif
			db->add_database(rdb);
		}
	}

	schema.setDatabase(this);
}


bool
Database::commit()
{
	schema.store();

	for (int t = 3; t >= 0; --t) {
		L_DATABASE_WRAP(this, "Commit: t: %d", t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->commit();
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "Commit made");
		modified = false;
		return true;
	}

	L_ERR(this, "ERROR: Not can do commit!");
	return false;
}


bool
Database::drop(const std::string& doc_id, bool _commit)
{
	if (!(flags & DB_WRITABLE)) {
		L_ERR(this, "ERROR: database is read-only");
		return 0;
	}

	std::string document_id = prefixed(doc_id, DOCUMENT_ID_TERM_PREFIX);

	for (int t = 3; t >= 0; --t) {
		L_DATABASE_WRAP(this, "Deleting doc_id: %s  t: %d", document_id.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->delete_document(document_id);
		} catch (const Xapian::DatabaseCorruptError& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			return false;
		} catch (const Xapian::DatabaseError& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			return false;
		} catch (const Xapian::Error& e) {
			L_DEBUG(this, "Inside catch drop");
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "Document deleted");
		if (_commit) {
			return commit();
		} else {
			modified = true;
			return true;
		}
	}

	L_ERR(this, "ERROR: Not can delete document: %s!", document_id.c_str());
	return false;
}


void
Database::index_required_data(Xapian::Document& doc, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length) const
{
	std::size_t found = ct_type.find_last_of("/");
	std::string type(ct_type.c_str(), found);
	std::string subtype(ct_type.c_str(), found + 1, ct_type.size());

	// Saves document's id in DB_SLOT_ID
	doc.add_value(DB_SLOT_ID, _document_id);

	// Document's id is also a boolean term (otherwise it doesn't replace an existing document)
	term_id = prefixed(_document_id, DOCUMENT_ID_TERM_PREFIX);
	doc.add_boolean_term(term_id);
	L_DATABASE_WRAP(this, "Slot: 0 _id: %s  term: %s", _document_id.c_str(), term_id.c_str());

	// Indexing the content values of data.
	doc.add_value(DB_SLOT_OFFSET, DEFAULT_OFFSET);
	doc.add_value(DB_SLOT_TYPE, ct_type);
	doc.add_value(DB_SLOT_LENGTH, ct_length);

	// Index terms for content-type
	std::string term_prefix = get_prefix("content_type", DOCUMENT_CUSTOM_TERM_PREFIX, STRING_TYPE);
	doc.add_term(prefixed(ct_type, term_prefix));
	doc.add_term(prefixed(type + "/*", term_prefix));
	doc.add_term(prefixed("*/" + subtype, term_prefix));
}


void
Database::index_object(Xapian::Document& doc, const std::string& str_key, const MsgPack& item_val, MsgPack&& properties, bool is_value)
{
	const specification_t spc_bef = schema.specification;
	if (item_val.obj->type == msgpack::type::MAP) {
		bool offsprings = false;
		for (auto subitem_key : item_val) {
			std::string str_subkey(subitem_key.obj->via.str.ptr, subitem_key.obj->via.str.size);
			auto subitem_val = item_val.at(str_subkey);
			if (!is_reserved(str_subkey)) {
				std::string full_subkey(str_key.empty() ? str_subkey : str_key + DB_OFFSPRING_UNION + str_subkey);
				index_object(doc, full_subkey, subitem_val, schema.get_subproperties(properties, str_subkey, subitem_val), is_value);
				offsprings = true;
			} else if (str_subkey.compare(RESERVED_VALUE) == 0) {
				if (schema.specification.sep_types[2] == NO_TYPE) {
					schema.set_type(properties, str_key, subitem_val);
				}
				if (is_value || schema.specification.index == Index::VALUE) {
					index_values(doc, str_key, subitem_val, properties);
				} else if (schema.specification.index == Index::TERM) {
					index_terms(doc, str_key, subitem_val, properties);
				} else {
					index_values(doc, str_key, subitem_val, properties, true);
				}
			}
			schema.specification = spc_bef;
		}
		if (offsprings) {
			schema.set_type_to_object(properties);
		}
	} else {
		if (schema.specification.sep_types[2] == NO_TYPE) {
			schema.set_type(properties, str_key, item_val);
		}
		if (is_value || schema.specification.index == Index::VALUE) {
			index_values(doc, str_key, item_val, properties);
		} else if (schema.specification.index == Index::TERM) {
			index_terms(doc, str_key, item_val, properties);
		} else {
			index_values(doc, str_key, item_val, properties, true);
		}
		schema.specification = spc_bef;
	}
}


void
Database::index_texts(Xapian::Document& doc, const std::string& name, const MsgPack& texts, MsgPack& properties)
{
	// L_DATABASE_WRAP(this, "Texts => Field: %s\nSpecifications: %s", name.c_str(), schema.specification.to_string().c_str());
	if (!(schema.found_field || schema.specification.dynamic)) {
		throw MSG_Error("%s is not dynamic", name.c_str());
	}

	if (schema.specification.store) {
		if (schema.specification.bool_term) {
			throw MSG_Error("A boolean term can not be indexed as text");
		}

		try {
			if (texts.obj->type == msgpack::type::ARRAY) {
				schema.set_type_to_array(properties);
				size_t pos = 0;
				for (auto text : texts) {
					index_text(doc, text.get_str(), pos++);
				}
			} else {
				index_text(doc, texts.get_str(), 0);
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Texts should be a string or array of strings");
		}
	}
}


void
Database::index_text(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	const Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());

	Xapian::TermGenerator term_generator;
	term_generator.set_document(doc);
	term_generator.set_stemmer(Xapian::Stem(schema.specification.language[getPos(pos, schema.specification.language.size())]));
	if (schema.specification.spelling[getPos(pos, schema.specification.spelling.size())]) {
		term_generator.set_database(*wdb);
		term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		term_generator.set_stemming_strategy((Xapian::TermGenerator::stem_strategy)schema.specification.analyzer[getPos(pos, schema.specification.analyzer.size())]);
	}

	if (schema.specification.positions[getPos(pos, schema.specification.positions.size())]) {
		if (schema.specification.prefix.empty()) {
			term_generator.index_text_without_positions(serialise_val, schema.specification.weight[getPos(pos, schema.specification.weight.size())]);
		} else {
			term_generator.index_text_without_positions(serialise_val, schema.specification.weight[getPos(pos, schema.specification.weight.size())], schema.specification.prefix);
		}
		L_DATABASE_WRAP(this, "Text index with positions => %s: %s", schema.specification.prefix.c_str(), serialise_val.c_str());
	} else {
		if (schema.specification.prefix.empty()) {
			term_generator.index_text(serialise_val, schema.specification.weight[getPos(pos, schema.specification.weight.size())]);
		} else {
			term_generator.index_text(serialise_val, schema.specification.weight[getPos(pos, schema.specification.weight.size())], schema.specification.prefix);
		}
		L_DATABASE_WRAP(this, "Text to Index = %s: %s", schema.specification.prefix.c_str(), serialise_val.c_str());
	}
}


void
Database::index_terms(Xapian::Document& doc, const std::string& name, const MsgPack& terms, MsgPack& properties)
{
	// L_DATABASE_WRAP(this, "Terms => Field: %s\nSpecifications: %s", name.c_str(), schema.specification.to_string().c_str());
	if (!(schema.found_field || schema.specification.dynamic)) {
		throw MSG_Error("%s is not dynamic", name.c_str());
	}

	if (schema.specification.store) {
		if (terms.obj->type == msgpack::type::ARRAY) {
			schema.set_type_to_array(properties);
			size_t pos = 0;
			for (auto term : terms) {
				index_term(doc, Serialise::serialise(schema.specification.sep_types[2], term), pos++);
			}
		} else {
			index_term(doc, Serialise::serialise(schema.specification.sep_types[2], terms), 0);
		}
	}
}


void
Database::index_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	if (schema.specification.sep_types[2] == STRING_TYPE && !schema.specification.bool_term) {
		if (serialise_val.find(" ") != std::string::npos) {
			return index_text(doc, std::move(serialise_val), pos);
		}
		to_lower(serialise_val);
	}

	L_DATABASE_WRAP(this, "Term[%d] -> %s: %s", pos, schema.specification.prefix.c_str(), serialise_val.c_str());
	std::string nameterm(prefixed(serialise_val, schema.specification.prefix));
	unsigned position = schema.specification.position[getPos(pos, schema.specification.position.size())];
	if (position) {
		if (schema.specification.bool_term) {
			doc.add_posting(nameterm, position, 0);
		} else {
			doc.add_posting(nameterm, position, schema.specification.weight[getPos(pos, schema.specification.weight.size())]);
		}
		L_DATABASE_WRAP(this, "Bool: %s  Posting: %s", schema.specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	} else {
		if (schema.specification.bool_term) {
			doc.add_boolean_term(nameterm);
		} else {
			doc.add_term(nameterm, schema.specification.weight[getPos(pos, schema.specification.weight.size())]);
		}
		L_DATABASE_WRAP(this, "Bool: %s  Term: %s", schema.specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	}
}


void
Database::index_values(Xapian::Document& doc, const std::string& name, const MsgPack& values, MsgPack& properties, bool is_term)
{
	// L_DATABASE_WRAP(this, "Values => Field: %s\nSpecifications: %s", name.c_str(), schema.specification.to_string().c_str());
	if (!(schema.found_field || schema.specification.dynamic)) {
		throw MSG_Error("%s is not dynamic", name.c_str());
	}

	if (schema.specification.store) {
		StringList s;
		size_t pos = 0;
		if (values.obj->type == msgpack::type::ARRAY) {
			schema.set_type_to_array(properties);
			s.reserve(values.obj->via.array.size);
			for (auto value : values) {
				index_value(doc, value, s, pos, is_term);
			}
		} else {
			index_value(doc, values, s, pos, is_term);
		}
		doc.add_value(schema.specification.slot, s.serialise());
		L_DATABASE_WRAP(this, "Slot: %u serialized: %s", schema.specification.slot, repr(s.serialise()).c_str());
	}
}


void
Database::index_value(Xapian::Document& doc, const MsgPack& value, StringList& s, size_t& pos, bool is_term) const
{
	std::string value_v;

	// Index terms generated by accuracy.
	switch (schema.specification.sep_types[2]) {
		case NUMERIC_TYPE: {
			try {
				double _value = value.get_f64();
				value_v = Serialise::numeric(NUMERIC_TYPE, _value);
				int64_t int_value = static_cast<int64_t>(_value);
				auto it = schema.specification.acc_prefix.begin();
				for (const auto& acc : schema.specification.accuracy) {
					std::string term_v = Serialise::numeric(NUMERIC_TYPE, int_value - int_value % (uint64_t)acc);
					doc.add_term(prefixed(term_v, *(it++)));
				}
				s.push_back(value_v);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_Error("Format invalid for numeric type");
			}
		}
		case DATE_TYPE: {
			Datetime::tm_t tm;
			value_v = Serialise::date(value, tm);
			auto it = schema.specification.acc_prefix.begin();
			for (const auto& acc : schema.specification.accuracy) {
				switch ((unitTime)acc) {
					case unitTime::YEAR:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "y"), *it++));
						break;
					case unitTime::MONTH:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "M"), *it++));
						break;
					case unitTime::DAY:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "d"), *it++));
						break;
					case unitTime::HOUR:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "h"), *it++));
						break;
					case unitTime::MINUTE:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "m"), *it++));
						break;
					case unitTime::SECOND:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "s"), *it++));
						break;
				}
			}
			s.push_back(value_v);
			break;
		}
		case GEO_TYPE: {
			std::string ewkt(value.get_str());
			if (is_term) {
				value_v = Serialise::ewkt(ewkt);
			}

			std::vector<range_t> ranges;
			CartesianList centroids;
			uInt64List start_end;

			EWKT_Parser::getRanges(ewkt, schema.specification.accuracy[0], schema.specification.accuracy[1], ranges, centroids);

			// Index Values and looking for terms generated by accuracy.
			std::set<std::string> set_terms;
			for (const auto& range : ranges) {
				start_end.push_back(range.start);
				start_end.push_back(range.end);
				int idx = -1;
				uint64_t val;
				if (range.start != range.end) {
					std::bitset<SIZE_BITS_ID> b1(range.start), b2(range.end), res;
					for (idx = SIZE_BITS_ID - 1; b1.test(idx) == b2.test(idx); --idx) {
						res.set(idx, b1.test(idx));
					}
					val = res.to_ullong();
				} else {
					val = range.start;
				}
				for (size_t i = 2; i < schema.specification.accuracy.size(); ++i) {
					int pos = START_POS - schema.specification.accuracy[i] * 2;
					if (idx < pos) {
						uint64_t vterm = val >> pos;
						set_terms.insert(prefixed(Serialise::trixel_id(vterm), schema.specification.acc_prefix[i - 2]));
					} else {
						break;
					}
				}
			}
			// Insert terms generated by accuracy.
			for (const auto& term : set_terms) {
				doc.add_term(term);
			}
			s.push_back(start_end.serialise());
			s.push_back(centroids.serialise());
			break;
		}
		default:
			value_v = Serialise::serialise(schema.specification.sep_types[2], value);
			s.push_back(value_v);
			break;
	}

	// Index like a term.
	if (is_term) {
		index_term(doc, std::move(value_v), pos++);
	}
}


void
Database::_index(Xapian::Document& doc, const MsgPack& obj)
{
	if (obj.obj->type == msgpack::type::MAP) {
		// Save a copy of schema for undo changes if there is a exception.
		auto str_schema = schema.to_string();
		auto _to_store = schema.getStore();
		try {
			auto properties = schema.getProperties();
			schema.update_root(properties, obj);

			const specification_t spc_bef = schema.specification;

			for (const auto item_key : obj) {
				std::string str_key(item_key.get_str());
				auto item_val = obj.at(str_key);
				if (!is_reserved(str_key)) {
					index_object(doc, str_key, item_val, schema.get_subproperties(properties, str_key, item_val), false);
				} else if (str_key == RESERVED_VALUES) {
					if (item_val.obj->type == msgpack::type::MAP) {
						index_object(doc, "", item_val, schema.getProperties());
					} else {
						throw MSG_Error("%s must be an object", RESERVED_VALUES);
					}
				} else if (str_key == RESERVED_TEXTS) {
					if (item_val.obj->type == msgpack::type::ARRAY) {
						for (auto subitem_val : item_val) {
							try {
								auto _value = subitem_val.at(RESERVED_VALUE);
								try {
									auto _name = subitem_val.at(RESERVED_NAME);
									try {
										std::string name(_name.get_str());
										auto subproperties = schema.get_subproperties(properties, name, subitem_val);
										if (schema.specification.sep_types[2] == NO_TYPE) {
											schema.set_type(subproperties, name, _value);
										}
										index_texts(doc, name, _value, subproperties);
									} catch (const msgpack::type_error&) {
										throw MSG_Error("%s must be string", RESERVED_NAME);
									}
								} catch (const std::out_of_range&) {
									schema.update_specification(subitem_val);
									MsgPack subproperties;
									if (schema.specification.sep_types[2] == NO_TYPE) {
										schema.set_type(subproperties, std::string(), _value);
									}
									index_texts(doc, std::string(), _value, subproperties);
								}
								schema.specification = spc_bef;
							} catch (const std::out_of_range&) {
								throw MSG_Error("%s must be defined in objects of %s", RESERVED_VALUE, RESERVED_TEXTS);
							} catch (const msgpack::type_error&) {
								throw MSG_Error("%s must be an array of objects", RESERVED_TEXTS);
							}
						}
					} else {
						throw MSG_Error("%s must be an array of objects", RESERVED_TEXTS);
					}
				} else if (str_key == RESERVED_TERMS) {
					if (item_val.obj->type == msgpack::type::ARRAY) {
						for (auto subitem_val : item_val) {
							try {
								auto _value = subitem_val.at(RESERVED_VALUE);
								try {
									auto _name = subitem_val.at(RESERVED_NAME);
									try {
										std::string name(_name.get_str());
										auto subproperties = schema.get_subproperties(properties, name, subitem_val);
										if (schema.specification.sep_types[2] == NO_TYPE) {
											schema.set_type(subproperties, name, _value);
										}
										index_terms(doc, name, _value, subproperties);
									} catch (const msgpack::type_error&) {
										throw MSG_Error("%s must be string", RESERVED_NAME);
									}
								} catch (const std::out_of_range&) {
									schema.update_specification(subitem_val);
									MsgPack subproperties;
									if (schema.specification.sep_types[2] == NO_TYPE) {
										schema.set_type(subproperties, std::string(), _value);
									}
									index_terms(doc, std::string(), _value, subproperties);
								}
								schema.specification = spc_bef;
							} catch (const std::out_of_range&) {
								throw MSG_Error("%s must be defined in objects of %s", RESERVED_VALUE, RESERVED_VALUES);
							} catch (const msgpack::type_error&) {
								throw MSG_Error("%s must be an array of objects", RESERVED_VALUES);
							}
						}
					} else {
						throw MSG_Error("%s must be an array of objects", RESERVED_VALUES);
					}
				}
			}
		} catch (const std::exception& err) {
			// Back to the initial schema if there are changes.
			if (schema.getStore()) {
				schema.setSchema(str_schema);
				schema.setStore(_to_store);
			}
			throw MSG_Error("%s", err.what());
		}
	}
}


Xapian::docid
Database::index(const std::string& body, const std::string& _document_id, bool _commit, const std::string& ct_type, const std::string& ct_length)
{
	if (!(flags & DB_WRITABLE)) {
		L_ERR(this, "ERROR: database is read-only");
		return 0;
	}

	if (_document_id.empty()) {
		L_ERR(this, "ERROR: Document must have an 'id'");
		return 0;
	}

	// Index required data for the document
	Xapian::Document doc;
	std::string term_id;
	index_required_data(doc, term_id, _document_id, ct_type, ct_length);

	// Create MsgPack object
	bool blob = true;
	MsgPack obj;
	rapidjson::Document rdoc;
	try {
		switch (get_mimetype(ct_type)) {
			case MIMEType::APPLICATION_JSON:
				json_load(rdoc, body);
				blob = false;
				obj = MsgPack(rdoc);
				break;
			case MIMEType::APPLICATION_XWWW_FORM_URLENCODED:
				try {
					json_load(rdoc, body);
					blob = false;
					obj = MsgPack(rdoc);
					doc.add_value(DB_SLOT_TYPE, JSON_TYPE);
				} catch (const std::exception&) { }
				break;
			case MIMEType::APPLICATION_X_MSGPACK:
				blob = false;
				obj = MsgPack(body);
				break;
			default:
				break;
		}

		L_DATABASE_WRAP(this, "Document to index: %s", body.c_str());
		std::string obj_str = obj.to_string();
		doc.set_data(encode_length(obj_str.size()) + obj_str + (blob ? body : ""));
		_index(doc, obj);
		L_DATABASE(this, "Schema: %s", schema.to_json_string().c_str());
		return replace(term_id, doc, _commit);
	} catch (const std::exception& err) {
		L_ERR(this, "ERROR: %s", err.what());
		return 0;
	}
}


Xapian::docid
Database::patch(const std::string& patches, const std::string& _document_id, bool _commit, const std::string& ct_type, const std::string& ct_length)
{
	if (!(flags & DB_WRITABLE)) {
		L_ERR(this, "ERROR: database is read-only");
		return 0;
	}

	if (_document_id.empty()) {
		L_ERR(this, "ERROR: Document must have an 'id'");
		return 0;
	}

	rapidjson::Document rdoc_patch;
	MIMEType t = get_mimetype(ct_type);
	MsgPack obj_patch;
	std::string _ct_type(ct_type);
	try {
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
				throw MSG_Error("Patches must be a JSON or MsgPack");
		}

		std::string prefix(DOCUMENT_ID_TERM_PREFIX);
		if (isupper(_document_id[0])) {
			prefix.append(":");
		}

		Xapian::QueryParser queryparser;
		queryparser.add_boolean_prefix(RESERVED_ID, prefix);
		auto query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + _document_id);

		Xapian::Enquire enquire(*db);
		enquire.set_query(query);
		Xapian::MSet mset = enquire.get_mset(0, 1);
		Xapian::MSetIterator m = mset.begin();

		Xapian::Document document;
		for (int t = 3; t >= 0; --t) {
			try {
				document = db->get_document(*m);
				break;
			} catch (const Xapian::InvalidArgumentError&) {
				return 0;
			} catch (const Xapian::DocNotFoundError&) {
				return 0;
			} catch (const Xapian::Error&) {
				reopen();
				m = mset.begin();
			}
		}

		MsgPack obj_data = get_MsgPack(document);
		if (apply_patch(obj_patch, obj_data)) {
			Xapian::Document doc;
			std::string term_id;

			// Index required data for the document
			index_required_data(doc, term_id, _document_id, _ct_type, ct_length);

			L_DATABASE_WRAP(this, "Document to index: %s", obj_data.to_json_string().c_str());
			std::string obj_data_str = obj_data.to_string();
			doc.set_data(encode_length(obj_data_str.size()) + obj_data_str + get_blob(document));
			_index(doc, obj_data);
			L_DATABASE(this, "Schema: %s", schema.to_json_string().c_str());
			return replace(term_id, doc, _commit);
		} else {
			// Object no patched
			return 0;
		}
	} catch (const std::exception& err) {
		L_ERR(this, "ERROR: %s", err.what());
		return 0;
	}
}


Xapian::docid
Database::replace(const std::string& document_id, const Xapian::Document& doc, bool _commit)
{
	Xapian::docid did;
	for (int t = 3; t >= 0; --t) {
		L_DATABASE_WRAP(this, "Replacing: %s  t: %d", document_id.c_str(), t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			did = wdb->replace_document(document_id, doc);
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "Document replaced");
		if (_commit) commit();
		else modified = true;
		return did;
	}

	return 0;
}


Xapian::docid
Database::replace(const Xapian::docid& did, const Xapian::Document& doc, bool _commit)
{
	for (int t = 3; t >= 0; --t) {
		L_DATABASE_WRAP(this, "Replacing did: %u t:%d", did, t);
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->replace_document(did, doc);
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "Document replaced");
		if (_commit) commit();
		else modified = true;
		return did;
	}

	return 0;
}


data_field_t
Database::get_data_field(const std::string& field_name)
{
	data_field_t res = { Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false };

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		auto properties = schema.getProperties().path(fields);

		res.type = properties.at(RESERVED_TYPE).at(2).get_u64();
		if (res.type == NO_TYPE) {
			return res;
		}

		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).get_u64());

		auto prefix = properties.at(RESERVED_PREFIX);
		res.prefix = prefix.get_str();

		res.bool_term = properties.at(RESERVED_BOOL_TERM).get_bool();

		// Strings and booleans do not have accuracy.
		if (res.type != STRING_TYPE && res.type != BOOLEAN_TYPE) {
			for (const auto acc : properties.at(RESERVED_ACCURACY)) {
				res.accuracy.push_back(acc.get_f64());
			}

			for (const auto acc_p : properties.at(RESERVED_ACC_PREFIX)) {
				res.acc_prefix.push_back(acc_p.get_str());
			}
		}
	} catch (const std::exception&) { }

	return res;
}


data_field_t
Database::get_slot_field(const std::string& field_name)
{
	data_field_t res = { Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false };

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		auto properties = schema.getProperties().path(fields);
		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).get_u64());
		res.type = properties.at(RESERVED_TYPE).at(2).get_u64();
	} catch (const std::exception&) { }

	return res;
}


Database::search_t
Database::_search(const std::string& query, unsigned flags, bool text, const std::string& lan)
{
	search_t srch;

	if (query.compare("*") == 0) {
		srch.query = Xapian::Query::MatchAll;
		srch.suggested_query.push_back("");
		return srch;
	}

	size_t size_match = 0;
	bool first_time = true, first_timeR = true;
	std::string querystring;
	Xapian::Query queryRange;
	Xapian::QueryParser queryparser;
	queryparser.set_database(*db);

	if (text) {
		queryparser.set_stemming_strategy(queryparser.STEM_SOME);
		lan.empty() ? queryparser.set_stemmer(Xapian::Stem(default_spc.language[0])) : queryparser.set_stemmer(Xapian::Stem(lan));
	}

	std::unordered_set<std::string> added_prefixes;
	std::unique_ptr<NumericFieldProcessor> nfp;
	std::unique_ptr<DateFieldProcessor> dfp;
	std::unique_ptr<GeoFieldProcessor> gfp;
	std::unique_ptr<BooleanFieldProcessor> bfp;

	std::sregex_iterator next(query.begin(), query.end(), find_field_re, std::regex_constants::match_continuous);
	std::sregex_iterator end;
	while (next != end) {
		std::string field(next->str(0));
		size_match += next->length(0);
		std::string field_name_dot(next->str(1));
		std::string field_name(next->str(2));
		std::string field_value(next->str(3));
		data_field_t field_t = get_data_field(field_name);

		std::smatch m;
		if (std::regex_match(field_value, m, find_range_re)) {
			// If this field is not indexed as value, not process this query.
			if (field_t.slot == Xapian::BAD_VALUENO) {
				++next;
				continue;
			}

			switch (field_t.type) {
				case NUMERIC_TYPE: {
					std::string filter_term, start(m.str(1)), end(m.str(2));
					std::vector<std::string> prefixes;
					GenerateTerms::numeric(filter_term, start, end, field_t.accuracy, field_t.acc_prefix, prefixes);
					queryRange = MultipleValueRange::getQuery(field_t.slot, NUMERIC_TYPE, start, end, field_name);
					if (!filter_term.empty()) {
						for (const auto& prefix : prefixes) {
							// Xapian does not allow repeat prefixes.
							if (added_prefixes.insert(prefix).second) {
								nfp = std::make_unique<NumericFieldProcessor>(prefix);
								queryparser.add_prefix(prefix, nfp.get());
								srch.nfps.push_back(std::move(nfp));
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
					}
					break;
				}
				case STRING_TYPE: {
					std::string start(m.str(1)), end(m.str(2));
					queryRange = MultipleValueRange::getQuery(field_t.slot, STRING_TYPE, start, end, field_name);
					break;
				}
				case DATE_TYPE: {
					std::string filter_term, start(m.str(1)), end(m.str(2));
					std::vector<std::string> prefixes;
					GenerateTerms::date(filter_term, start, end, field_t.accuracy, field_t.acc_prefix, prefixes);
					queryRange = MultipleValueRange::getQuery(field_t.slot, DATE_TYPE, start, end, field_name);
					if (!filter_term.empty()) {
						for (const auto& prefix : prefixes) {
							// Xapian does not allow repeat prefixes.
							if (added_prefixes.insert(prefix).second) {
								dfp = std::make_unique<DateFieldProcessor>(prefix);
								queryparser.add_prefix(prefix, dfp.get());
								srch.dfps.push_back(std::move(dfp));
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
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
					field_value.assign(field_value.c_str(), 3, field_value.size() - 4);
					std::vector<range_t> ranges;
					CartesianList centroids;
					EWKT_Parser::getRanges(field_value, field_t.accuracy[0], field_t.accuracy[1], ranges, centroids);

					queryRange = GeoSpatialRange::getQuery(field_t.slot, ranges, centroids);

					std::string filter_term;
					std::vector<std::string> prefixes;
					GenerateTerms::geo(filter_term, ranges, field_t.accuracy, field_t.acc_prefix, prefixes);
					if (!filter_term.empty()) {
						// Xapian does not allow repeat prefixes.
						for (const auto& prefix : prefixes) {
							// Xapian does not allow repeat prefixes.
							if (added_prefixes.insert(prefix).second) {
								gfp = std::make_unique<GeoFieldProcessor>(prefix);
								queryparser.add_prefix(prefix, gfp.get());
								srch.gfps.push_back(std::move(gfp));
							}
						}
						queryRange = Xapian::Query(Xapian::Query::OP_AND, queryparser.parse_query(filter_term, flags), queryRange);
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
				case NUMERIC_TYPE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						nfp = std::make_unique<NumericFieldProcessor>(field_t.prefix);
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, nfp.get()) : queryparser.add_prefix(field_name, nfp.get());
						srch.nfps.push_back(std::move(nfp));
					}
					field = field_name_dot + query_string(field_value);
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

					field = field_name_dot + query_string(std::to_string(Datetime::timestamp(field_value)));
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						dfp = std::make_unique<DateFieldProcessor>(field_t.prefix);
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, dfp.get()) : queryparser.add_prefix(field_name, dfp.get());
						srch.dfps.push_back(std::move(dfp));
					}
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

					field = field_name_dot + field_value;

					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						field_t.bool_term ? queryparser.add_boolean_prefix(field_name, field_t.prefix) : queryparser.add_prefix(field_name, field_t.prefix);
					}
					break;
				case BOOLEAN_TYPE:
					// Xapian does not allow repeat prefixes.
					if (added_prefixes.insert(field_t.prefix).second) {
						bfp = std::make_unique<BooleanFieldProcessor>(field_t.prefix);
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
		throw Xapian::QueryParserError("Query '" + query + "' contains errors.\n" );
	}

	switch (first_time << 1 | first_timeR) {
		case 0:
			try {
				srch.query = Xapian::Query(Xapian::Query::OP_OR, queryparser.parse_query(querystring, flags), srch.query);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError& er) {
				L_ERR(this, "ERROR: %s", er.get_msg().c_str());
				reopen();
				srch.query = Xapian::Query(Xapian::Query::OP_OR, queryparser.parse_query(querystring, flags), srch.query);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			}
			break;
		case 1:
			try {
				srch.query = queryparser.parse_query(querystring, flags);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			} catch (const Xapian::QueryParserError& er) {
				L_ERR(this, "ERROR: %s", er.get_msg().c_str());
				reopen();
				queryparser.set_database(*db);
				srch.query = queryparser.parse_query(querystring, flags);
				srch.suggested_query.push_back(queryparser.get_corrected_query_string());
			}
			break;
		case 2:
			srch.suggested_query.push_back("");
			break;
		case 3:
			srch.query = Xapian::Query::MatchNothing;
			srch.suggested_query.push_back("");
			break;
	}

	return srch;
}


Database::search_t
Database::search(const query_field_t& e)
{
	search_t srch_resul;
	std::vector<std::string> sug_query;
	bool first = true;

	L_DEBUG(this, "e.query size: %d  Spelling: %d Synonyms: %d", e.query.size(), e.spelling, e.synonyms);
	auto lit = e.language.begin();
	std::string lan;
	unsigned flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_PURE_NOT;
	if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
	if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
	Xapian::Query queryQ;
	for (const auto& query : e.query) {
		if (lit != e.language.end()) {
			lan = *lit++;
		}
		search_t srch = _search(query, flags, true, lan);
		if (first) {
			queryQ = srch.query;
			first = false;
		} else {
			queryQ =  Xapian::Query(Xapian::Query::OP_AND, queryQ, srch.query);
		}
		sug_query.push_back(srch.suggested_query.back());
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_DEBUG(this, "e.query: %s", queryQ.get_description().c_str());

	L_DEBUG(this, "e.partial size: %d", e.partial.size());
	flags = Xapian::QueryParser::FLAG_PARTIAL;
	if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
	if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
	first = true;
	Xapian::Query queryP;
	for (const auto& partial : e.partial) {
		search_t srch = _search(partial, flags, false, "");
		if (first) {
			queryP = srch.query;
			first = false;
		} else {
			queryP = Xapian::Query(Xapian::Query::OP_AND_MAYBE , queryP, srch.query);
		}
		sug_query.push_back(srch.suggested_query.back());
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_DEBUG(this, "e.partial: %s", queryP.get_description().c_str());

	L_DEBUG(this, "e.terms size: %d", e.terms.size());
	flags = Xapian::QueryParser::FLAG_BOOLEAN | Xapian::QueryParser::FLAG_PURE_NOT;
	if (e.spelling) flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
	if (e.synonyms) flags |= Xapian::QueryParser::FLAG_SYNONYM;
	first = true;
	Xapian::Query queryT;
	for (const auto& terms : e.terms) {
		search_t srch = _search(terms, flags, false, "");
		if (first) {
			queryT = srch.query;
			first = false;
		} else {
			queryT = Xapian::Query(Xapian::Query::OP_AND, queryT, srch.query);
		}
		sug_query.push_back(srch.suggested_query.back());
		srch_resul.nfps.insert(srch_resul.nfps.end(), std::make_move_iterator(srch.nfps.begin()), std::make_move_iterator(srch.nfps.end()));
		srch_resul.dfps.insert(srch_resul.dfps.end(), std::make_move_iterator(srch.dfps.begin()), std::make_move_iterator(srch.dfps.end()));
		srch_resul.gfps.insert(srch_resul.gfps.end(), std::make_move_iterator(srch.gfps.begin()), std::make_move_iterator(srch.gfps.end()));
		srch_resul.bfps.insert(srch_resul.bfps.end(), std::make_move_iterator(srch.bfps.begin()), std::make_move_iterator(srch.bfps.end()));
	}
	L_DEBUG(this, "e.terms: %s", repr(queryT.get_description()).c_str());

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
	srch_resul.suggested_query = sug_query;

	return srch_resul;
}


void
Database::get_similar(bool is_fuzzy, Xapian::Enquire& enquire, Xapian::Query& query, const similar_field_t& similar)
{
	Xapian::RSet rset;

	for (int t = 3; t >= 0; --t) {
		try {
			Xapian::Enquire renquire = get_enquire(query, Xapian::BAD_VALUENO, nullptr, nullptr, nullptr);
			Xapian::MSet mset = renquire.get_mset(0, similar.n_rset);
			for (const auto& doc : mset) {
				rset.add_document(doc);
			}
		} catch (const Xapian::Error& er) {
			L_ERR(this, "ERROR: %s", er.get_msg().c_str());
			if (t) reopen();
			continue;
		}

		std::vector<std::string> prefixes;
		prefixes.reserve(similar.type.size() + similar.field.size());
		for (const auto& sim_type : similar.type) {
			prefixes.push_back(DOCUMENT_CUSTOM_TERM_PREFIX + Unserialise::type(sim_type));
		}

		for (const auto& sim_field : similar.field) {
			data_field_t field_t = get_data_field(sim_field);
			prefixes.push_back(field_t.prefix);
		}

		ExpandDeciderFilterPrefixes efp(prefixes);
		Xapian::ESet eset = enquire.get_eset(similar.n_eset, rset, &efp);

		if (is_fuzzy) {
			query = Xapian::Query(Xapian::Query::OP_OR, query, Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar.n_term));
		} else {
			query = Xapian::Query(Xapian::Query::OP_ELITE_SET, eset.begin(), eset.end(), similar.n_term);
		}

		return;
	}
}


Xapian::Enquire
Database::get_enquire(Xapian::Query& query, const Xapian::valueno& collapse_key, const query_field_t *e, Multi_MultiValueKeyMaker *sorter,
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> *spies)
{
	Xapian::Enquire enquire(*db);

	enquire.set_query(query);

	if (sorter) {
		enquire.set_sort_by_key_then_relevance(sorter, false);
	}

	int collapse_max = 1;
	if (e) {
		if (e->is_nearest) {
			get_similar(false, enquire, query, e->nearest);
		}

		if (e->is_fuzzy) {
			get_similar(true, enquire, query, e->fuzzy);
		}

		for (const auto& facet : e->facets) {
			data_field_t field_t = get_slot_field(facet);
			if (field_t.type != NO_TYPE) {
				std::unique_ptr<MultiValueCountMatchSpy> spy = std::make_unique<MultiValueCountMatchSpy>(get_slot(facet), field_t.type == GEO_TYPE);
				enquire.add_matchspy(spy.get());
				L_ERR(this, "added spy de -%s-", (facet).c_str());
				spies->push_back(std::make_pair(facet, std::move(spy)));
			}
		}

		collapse_max = e->collapse_max;
	}

	if (collapse_key != Xapian::BAD_VALUENO) {
		enquire.set_collapse_key(collapse_max);
	}

	return enquire;
}


int
Database::get_mset(const query_field_t& e, Xapian::MSet& mset, std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>& spies,
		std::vector<std::string>& suggestions, int offset)
{
	auto doccount = db->get_doccount();
	auto check_at_least = std::max(std::min(doccount, e.check_at_least), 0u);
	Xapian::valueno collapse_key;

	// Get the collapse key to use for queries.
	if (!e.collapse.empty()) {
		data_field_t field_t = get_slot_field(e.collapse);
		collapse_key = field_t.slot;
	} else {
		collapse_key = Xapian::BAD_VALUENO;
	}

	Multi_MultiValueKeyMaker sorter_obj;
	Multi_MultiValueKeyMaker *sorter = nullptr;
	if (!e.sort.empty()) {
		sorter = &sorter_obj;
		for (const auto& sort : e.sort) {
			std::string field, value;
			size_t pos = sort.find(":");
			if (pos != std::string::npos) {
				field = sort.substr(0, pos);
				value = sort.substr(pos + 1);
			} else {
				field = sort;
			}

			if (field.at(0) == '-') {
				field.assign(field, 1, field.size() - 1);
				data_field_t field_t = get_slot_field(field);
				if (field_t.type != NO_TYPE) {
					sorter->add_value(field_t.slot, field_t.type, value, true);
				}
			} else if (field.at(0) == '+') {
				field.assign(field, 1, field.size() - 1);
				data_field_t field_t = get_slot_field(field);
				if (field_t.type != NO_TYPE) {
					sorter->add_value(field_t.slot, field_t.type, value);
				}
			} else {
				data_field_t field_t = get_slot_field(field);
				if (field_t.type != NO_TYPE) {
					sorter->add_value(field_t.slot, field_t.type, value);
				}
			}
		}
	}

	for (int t = 3; t >= 0; --t) {
		try {
			search_t srch = search(e);
			Xapian::Enquire enquire = get_enquire(srch.query, collapse_key, &e, sorter, &spies);
			suggestions = srch.suggested_query;
			mset = enquire.get_mset(e.offset + offset, e.limit - offset, check_at_least);
		} catch (const Xapian::DatabaseModifiedError& er) {
			L_ERR(this, "ERROR: %s", er.get_msg().c_str());
			if (t) reopen();
			continue;
		} catch (const Xapian::NetworkError& er) {
			L_ERR(this, "ERROR: %s", er.get_msg().c_str());
			if (t) reopen();
			continue;
		} catch (const Xapian::Error& er) {
			L_ERR(this, "ERROR: %s", er.get_msg().c_str());
			return 2;
		} catch (const std::exception& er) {
			L_DATABASE_WRAP(this, "ERROR: %s", er.what());
			return 1;
		}

		return 0;
	}

	L_ERR(this, "ERROR: The search was not performed!");
	return 2;
}


bool
Database::get_metadata(const std::string& key, std::string& value)
{
	for (int t = 3; t >= 0; --t) {
		try {
			value = db->get_metadata(key);
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "get_metadata was done");
		return value.empty() ? false : true;
	}

	L_ERR(this, "ERROR: get_metadata can not be done!");
	return false;
}


bool
Database::set_metadata(const std::string& key, const std::string& value, bool _commit)
{
	for (int t = 3; t >= 0; --t) {
		Xapian::WritableDatabase *wdb = static_cast<Xapian::WritableDatabase *>(db.get());
		try {
			wdb->set_metadata(key, value);
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		L_DATABASE_WRAP(this, "set_metadata was done");
		return (_commit) ? commit() : true;
	}

	L_ERR(this, "ERROR: set_metadata can not be done!");
	return false;
}


bool
Database::get_document(const Xapian::docid& did, Xapian::Document& doc)
{
	for (int t = 3; t >= 0; --t) {
		try {
			doc = db->get_document(did);
		} catch (const Xapian::Error& e) {
			L_ERR(this, "ERROR: %s", e.get_msg().c_str());
			if (t) reopen();
			continue;
		}
		return true;
	}

	return false;
}


void
Database::get_stats_database(MsgPack&& stats)
{
	unsigned doccount = db->get_doccount();
	unsigned lastdocid = db->get_lastdocid();
	stats["uuid"] = db->get_uuid();
	stats["doc_count"] = doccount;
	stats["last_id"] = lastdocid;
	stats["doc_del"] = lastdocid - doccount;
	stats["av_length"] = db->get_avlength();
	stats["doc_len_lower"] =  db->get_doclength_lower_bound();
	stats["doc_len_upper"] = db->get_doclength_upper_bound();
	stats["has_positions"] = db->has_positions();
}


void
Database::get_stats_docs(MsgPack&& stats, const std::string& document_id)
{
	std::string prefix(DOCUMENT_ID_TERM_PREFIX);
	if (isupper(document_id.at(0))) {
		prefix += ":";
	}

	Xapian::QueryParser queryparser;
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	Xapian::Query query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + document_id);

	Xapian::Enquire enquire(*db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);
	Xapian::MSetIterator m = mset.begin();

	stats[RESERVED_ID] = document_id;

	Xapian::Document doc;
	for (int t = 3; t >= 0; --t) {
		try {
			doc = db->get_document(*m);
			break;
		} catch (Xapian::InvalidArgumentError&) {
			stats["error"] = "Invalid internal document id";
			return;
		} catch (Xapian::DocNotFoundError&) {
			stats["error"] = "Document not found";
			return;
		} catch (const Xapian::Error&) {
			reopen();
			m = mset.begin();
		}
	}

	MsgPack obj_data = get_MsgPack(doc);
	try {
		obj_data = obj_data.at(RESERVED_DATA);
	} catch (const std::out_of_range&) {
		clean_reserved(obj_data);
		obj_data[RESERVED_ID] = doc.get_value(DB_SLOT_ID);
	}

	stats[RESERVED_DATA] = std::move(obj_data);

	std::string ct_type = doc.get_value(DB_SLOT_TYPE);
	stats["has_blob"] = ct_type != JSON_TYPE && ct_type != MSGPACK_TYPE;

	stats["number_terms"] = doc.termlist_count();

	std::string terms;
	const auto it_e = doc.termlist_end();
	for (auto it = doc.termlist_begin(); it != it_e; ++it) {
		terms += repr(*it) + " ";
	}
	stats[RESERVED_TERMS] = terms;

	stats["number_values"] = doc.values_count();

	std::string values;
	const auto iv_e = doc.values_end();
	for (auto iv = doc.values_begin(); iv != iv_e; ++iv) {
		values += std::to_string(iv.get_valueno()) + ":" + repr(*iv) + " ";
	}
	stats[RESERVED_VALUES] = values;
}


DatabaseQueue::DatabaseQueue()
	: state(replica_state::REPLICA_FREE),
	  persistent(false),
	  count(0) { }


DatabaseQueue::DatabaseQueue(DatabaseQueue&& q)
{
	std::lock_guard<std::mutex> lk(q._mutex);
	_items_queue = std::move(q._items_queue);
	_limit = std::move(q._limit);
	state = std::move(q.state);
	persistent = std::move(q.persistent);
	count = std::move(q.count);
	weak_database_pool = std::move(q.weak_database_pool);
}


DatabaseQueue::~DatabaseQueue()
{
	assert(size() == count);
}


bool
DatabaseQueue::inc_count(int max)
{
	std::unique_lock<std::mutex> lk(_mutex);

	if (count == 0) {
		if (auto database_pool = weak_database_pool.lock()) {
			for (auto& endpoint : endpoints) {
				database_pool->add_endpoint_queue(endpoint, shared_from_this());
			}
		}
	}

	if (max == -1 || count < static_cast<size_t>(max)) {
		++count;
		return true;
	}

	return false;
}


bool
DatabaseQueue::dec_count()
{
	std::unique_lock<std::mutex> lk(_mutex);

	assert(count > 0);

	if (count > 0) {
		--count;
		return true;
	}

	if (auto database_pool = weak_database_pool.lock()) {
		for (auto& endpoint : endpoints) {
			database_pool->drop_endpoint_queue(endpoint, shared_from_this());
		}
	}

	return false;
}


DatabasePool::DatabasePool(size_t max_size)
	: finished(false),
	  databases(max_size),
	  writable_databases(max_size) { }


DatabasePool::~DatabasePool()
{
	finish();
}


void
DatabasePool::add_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.insert(queue);
}


void
DatabasePool::drop_endpoint_queue(const Endpoint& endpoint, const std::shared_ptr<DatabaseQueue>& queue)
{
	size_t hash = endpoint.hash();
	auto& queues_set = queues[hash];
	queues_set.erase(queue);

	if (queues_set.empty()) {
		queues.erase(hash);
	}
}


long long
DatabasePool::get_mastery_level(const std::string& dir)
{
	Endpoints endpoints;
	endpoints.insert(Endpoint(dir));

	std::shared_ptr<Database> database;
	if (checkout(database, endpoints, 0)) {
		long long mastery_level = database->mastery_level;
		checkin(database);
		return mastery_level;
	}

	return read_mastery(dir, false);
}


void
DatabasePool::finish()
{
	finished = true;
}


bool DatabasePool::checkout(std::shared_ptr<Database>& database, const Endpoints& endpoints, int flags)
{
	bool writable = flags & DB_WRITABLE;
	bool persistent = flags & DB_PERSISTENT;
	bool initref = flags & DB_INIT_REF;
	bool replication = flags & DB_REPLICATION;

	L_DATABASE_BEGIN(this, "++ CHECKING OUT DB %s(%s) [%lx]...", writable ? "w" : "r", endpoints.as_string().c_str(), (unsigned long)database.get());

	assert(!database);

	std::unique_lock<std::mutex> lk(qmtx);

	if (!finished) {
		size_t hash = endpoints.hash();

		std::shared_ptr<DatabaseQueue> queue;
		if (writable) {
			queue = writable_databases[hash];
		} else {
			queue = databases[hash];
		}

		auto old_state = queue->state;

		if (replication) {
			switch (queue->state) {
				case DatabaseQueue::replica_state::REPLICA_FREE:
					queue->state = DatabaseQueue::replica_state::REPLICA_LOCK;
					break;
				case DatabaseQueue::replica_state::REPLICA_LOCK:
				case DatabaseQueue::replica_state::REPLICA_SWITCH:
					L_REPLICATION(this, "A replication task is already waiting");
					L_DATABASE_END(this, "!! ABORTED CHECKOUT DB (%s)!", endpoints.as_string().c_str());
					return false;
			}
		} else {
			if (queue->state == DatabaseQueue::replica_state::REPLICA_SWITCH) {
				queue->switch_cond.wait(lk);
			}
		}

		bool old_persistent = queue->persistent;
		queue->persistent = persistent;

		if (!queue->pop(database, 0)) {
			// Increment so other threads don't delete the queue
			if (queue->inc_count(writable ? 1 : -1)) {
				lk.unlock();
				try {
					database = std::make_shared<Database>(queue, endpoints, flags);

					if (writable && initref && endpoints.size() == 1) {
						init_ref(endpoints);
					}

				} catch (const Xapian::DatabaseOpeningError& err) {
				} catch (const Xapian::Error& err) {
					L_ERR(this, "ERROR: %s", err.get_msg().c_str());
				}
				lk.lock();
				queue->dec_count();  // Decrement, count should have been already incremented if Database was created
			} else {
				// Lock until a database is available if it can't get one.
				lk.unlock();
				int s = queue->pop(database);
				lk.lock();
				if (!s) {
					L_ERR(this, "ERROR: Database is not available. Writable: %d", writable);
				}
			}
		}
		if (!database) {
			queue->state = old_state;
			queue->persistent = old_persistent;
			if (queue->count == 0) {
				// There was an error and the queue ended up being empty, remove it!
				databases.erase(hash);
			}
		}
	}

	lk.unlock();

	if (database) {
		database->modified = false;
	} else {
		L_DATABASE_END(this, "!! FAILED CHECKOUT DB (%s)!", endpoints.as_string().c_str());
		return false;
	}

	if (!writable && duration_cast<seconds>(system_clock::now() -  database->access_time).count() >= DATABASE_UPDATE_TIME) {
		database->reopen();
		L_DATABASE(this, "== REOPEN DB %s(%s) [%lx]", (database->flags & DB_WRITABLE) ? "w" : "r", database->endpoints.as_string().c_str(), (unsigned long)database.get());
	}

#ifdef HAVE_REMOTE_PROTOCOL
	if (database->local) {
		database->checkout_revision = database->db->get_revision_info();
	}
#endif
	L_DATABASE_END(this, "++ CHECKED OUT DB %s(%s), %s at rev:%s %lx", writable ? "w" : "r", endpoints.as_string().c_str(), database->local ? "local" : "remote", repr(database->checkout_revision, false).c_str(), (unsigned long)database.get());
	return true;
}


void
DatabasePool::checkin(std::shared_ptr<Database>& database)
{
	L_DATABASE_BEGIN(this, "-- CHECKING IN DB %s(%s) [%lx]...", (database->flags & DB_WRITABLE) ? "w" : "r", database->endpoints.as_string().c_str(), (unsigned long)database.get());

	assert(database);

	std::unique_lock<std::mutex> lk(qmtx);

	std::shared_ptr<DatabaseQueue> queue;

	if (database->flags & DB_WRITABLE) {
		queue = writable_databases[database->hash];
#ifdef HAVE_REMOTE_PROTOCOL
		if (database->local && database->mastery_level != -1) {
			std::string new_revision = database->db->get_revision_info();
			if (new_revision != database->checkout_revision) {
				Endpoint endpoint = *database->endpoints.begin();
				endpoint.mastery_level = database->mastery_level;
				updated_databases.push(endpoint);
			}
		}
#endif
	} else {
		queue = databases[database->hash];
	}

	assert(database->weak_queue.lock() == queue);

	int flags = database->flags;
	Endpoints &endpoints = database->endpoints;

	if (database->modified) {
		DatabaseAutocommit::signal_changed(database);
	}

	if (!(flags & DB_VOLATILE)) {
		queue->push(database);
	}

	bool signal_checkins = false;
	switch (queue->state) {
		case DatabaseQueue::replica_state::REPLICA_SWITCH:
			for (auto& endpoint : endpoints) {
				_switch_db(endpoint);
			}
			if (queue->state == DatabaseQueue::replica_state::REPLICA_FREE) {
				signal_checkins = true;
			}
			break;
		case DatabaseQueue::replica_state::REPLICA_LOCK:
			queue->state = DatabaseQueue::replica_state::REPLICA_FREE;
			signal_checkins = true;
			break;
		case DatabaseQueue::replica_state::REPLICA_FREE:
			break;
	}

	assert(queue->count >= queue->size());

	L_DATABASE_END(this, "-- CHECKED IN DB %s(%s) [%lx]", (flags & DB_WRITABLE) ? "w" : "r", endpoints.as_string().c_str(), (unsigned long)database.get());

	database.reset();

	lk.unlock();

	if (signal_checkins) {
		while (queue->checkin_callbacks.call());
	}
}


bool
DatabasePool::_switch_db(const Endpoint& endpoint)
{
	auto queues_set = queues[endpoint.hash()];

	bool switched = true;
	for (auto& queue : queues_set) {
		queue->state = DatabaseQueue::replica_state::REPLICA_SWITCH;
		if (queue->count != queue->size()) {
			switched = false;
			break;
		}
	}

	if (switched) {
		move_files(endpoint.path + "/.tmp", endpoint.path);

		for (auto& queue : queues_set) {
			queue->state = DatabaseQueue::replica_state::REPLICA_FREE;
			queue->switch_cond.notify_all();
		}
	} else {
		L_DEBUG(this, "Inside switch_db not queue->count == queue->size()");
	}

	return switched;
}


bool
DatabasePool::switch_db(const Endpoint& endpoint)
{
	std::lock_guard<std::mutex> lk(qmtx);
	return _switch_db(endpoint);
}


void
DatabasePool::init_ref(const Endpoints& endpoints)
{
	Endpoints ref_endpoints;
	ref_endpoints.insert(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
		L_ERR(this, "Database refs it could not be checkout.");
		assert(false);
	}

	for (auto endp_it = endpoints.begin(); endp_it != endpoints.end(); ++endp_it) {
		std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
		Xapian::PostingIterator p(ref_database->db->postlist_begin(unique_id));
		if (p == ref_database->db->postlist_end(unique_id)) {
			Xapian::Document doc;
			// Boolean term for the node.
			doc.add_boolean_term(unique_id);
			// Start values for the DB.
			doc.add_boolean_term(prefixed(DB_MASTER, get_prefix("master", DOCUMENT_CUSTOM_TERM_PREFIX, STRING_TYPE)));
			doc.add_value(DB_SLOT_CREF, "0");
			ref_database->replace(unique_id, doc, true);
		}
	}

	checkin(ref_database);
}


void
DatabasePool::inc_ref(const Endpoints& endpoints)
{
	Endpoints ref_endpoints;
	ref_endpoints.insert(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
		L_ERR(this, "Database refs it could not be checkout.");
		assert(false);
	}

	Xapian::Document doc;

	for (auto endp_it = endpoints.begin(); endp_it != endpoints.end(); ++endp_it) {
		std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
		if (p == ref_database->db->postlist_end(unique_id)) {
			// QUESTION: Document not found - should add?
			// QUESTION: This case could happen?
			doc.add_boolean_term(unique_id);
			doc.add_value(0, "0");
			ref_database->replace(unique_id, doc, true);
		} else {
			// Document found - reference increased
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			int nref = std::stoi(doc.get_value(0));
			doc.add_value(0, std::to_string(nref + 1));
			ref_database->replace(unique_id, doc, true);
		}
	}

	checkin(ref_database);
}


void
DatabasePool::dec_ref(const Endpoints& endpoints)
{
	Endpoints ref_endpoints;
	ref_endpoints.insert(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
		L_ERR(this, "Database refs it could not be checkout.");
		assert(false);
	}

	Xapian::Document doc;

	for (auto endp_it = endpoints.begin(); endp_it != endpoints.end(); ++endp_it) {
		std::string unique_id(prefixed(get_slot_hex(endp_it->path), DOCUMENT_ID_TERM_PREFIX));
		Xapian::PostingIterator p = ref_database->db->postlist_begin(unique_id);
		if (p != ref_database->db->postlist_end(unique_id)) {
			doc = ref_database->db->get_document(*p);
			doc.add_boolean_term(unique_id);
			int nref = std::stoi(doc.get_value(0)) - 1;
			doc.add_value(0, std::to_string(nref));
			ref_database->replace(unique_id, doc, true);
			if (nref == 0) {
				// qmtx need a lock
				delete_files(endp_it->path);
			}
		}
	}

	checkin(ref_database);
}


int
DatabasePool::get_master_count()
{
	Endpoints ref_endpoints;
	ref_endpoints.insert(Endpoint(".refs"));
	std::shared_ptr<Database> ref_database;
	if (!checkout(ref_database, ref_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
		L_ERR(this, "Database refs it could not be checkout.");
		assert(false);
	}

	int count = 0;

	if (ref_database) {
		Xapian::PostingIterator p(ref_database->db->postlist_begin(DB_MASTER));
		count = std::distance(ref_database->db->postlist_begin(DB_MASTER), ref_database->db->postlist_end(DB_MASTER));
	}

	checkin(ref_database);

	return count;
}


bool
ExpandDeciderFilterPrefixes::operator()(const std::string& term) const
{
	for (const auto& prefix : prefixes) {
		if (startswith(term, prefix)) {
			return true;
		}
	}

	return prefixes.empty();
}
