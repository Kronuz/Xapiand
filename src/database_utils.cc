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

#include "database_utils.h"

#include "datetime.h"
#include "io_utils.h"
#include "length.h"
#include "log.h"
#include "manager.h"
#include "msgpack_patcher.h"
#include "schema.h"
#include "serialise.h"
#include "wkt_parser.h"

#include "rapidjson/error/en.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>

#define DATABASE_DATA_HEADER_MAGIC 0x42
#define DATABASE_DATA_FOOTER_MAGIC 0x2A


const std::regex find_types_re("(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" NUMERIC_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")", std::regex::icase | std::regex::optimize);


long long save_mastery(const std::string& dir) {
	char buf[20];
	long long mastery_level = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << 16;
	mastery_level |= static_cast<int>(random_int(0, 0xffff));
	int fd = io::open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
	if (fd >= 0) {
		snprintf(buf, sizeof(buf), "%llx", mastery_level);
		io::write(fd, buf, strlen(buf));
		io::close(fd);
	}
	return mastery_level;
}


long long read_mastery(const std::string& dir, bool force) {
	L_DATABASE(nullptr, "+ READING MASTERY OF INDEX '%s'...", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		L_DATABASE(nullptr, "- NO MASTERY OF INDEX '%s'", dir.c_str());
		return -1;
	}

	long long mastery_level = -1;

	int fd = io::open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		if (force) {
			mastery_level = save_mastery(dir);
		}
	} else {
		char buf[20];
		mastery_level = 0;
		size_t length = io::read(fd, buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			mastery_level = std::stoll(buf, nullptr, 16);
		}
		io::close(fd);
		if (!mastery_level) {
			mastery_level = save_mastery(dir);
		}
	}

	L_DATABASE(nullptr, "- MASTERY OF INDEX '%s' is %llx", dir.c_str(), mastery_level);

	return mastery_level;
}


bool is_valid(const std::string& word) {
	return word.front() != '_' && word.back() != '_';
}


bool is_language(const std::string& language) {
	if (language.find(" ") == std::string::npos) {
		return std::string(DB_LANGUAGES).find(language) == std::string::npos ? false : true;
	}
	return false;
}


bool set_types(const std::string& type, std::vector<unsigned>& sep_types) {
	std::smatch m;
	if (std::regex_match(type, m, find_types_re) && static_cast<size_t>(m.length(0)) == type.size()) {
		if (m.length(4) != 0) {
			sep_types[0] = OBJECT_TYPE;
			sep_types[1] = NO_TYPE;
			sep_types[2] = NO_TYPE;
		} else {
			if (m.length(1) != 0) {
				sep_types[0] = OBJECT_TYPE;
			}
			if (m.length(2) != 0) {
				sep_types[1] = ARRAY_TYPE;
			}
			sep_types[2] = m.str(3).at(0);
		}
		return true;
	}

	return false;
}


std::string str_type(const std::vector<unsigned>& sep_types) {
	std::stringstream str;
	if (sep_types[0] == OBJECT_TYPE) str << OBJECT_STR << "/";
	if (sep_types[1] == ARRAY_TYPE) str << ARRAY_STR << "/";
	str << Serialise::type(sep_types[2]);
	return str.str();
}


void clean_reserved(MsgPack& document) {
	if (document.get_type() == msgpack::type::MAP) {
		for (auto item_key : document) {
			std::string str_key(item_key.get_str());
			if (is_valid(str_key) || str_key == RESERVED_VALUE) {
				auto item_doc = document.at(str_key);
				clean_reserved(item_doc);
			} else {
				document.erase(str_key);
			}
		}
	}
}


MIMEType get_mimetype(const std::string& type) {
	if (type == JSON_TYPE) {
		return MIMEType::APPLICATION_JSON;
	} else if (type == FORM_URLENCODED_TYPE) {
		return MIMEType::APPLICATION_XWWW_FORM_URLENCODED;
	} else if (type == MSGPACK_TYPE) {
		return MIMEType::APPLICATION_X_MSGPACK;
	} else {
		return MIMEType::UNKNOW;
	}
}


void json_load(rapidjson::Document& doc, const std::string& str) {
	rapidjson::ParseResult parse_done = doc.Parse(str.data());
	if (!parse_done) {
		throw MSG_ClientError("JSON parse error at position %u: %s", parse_done.Offset(), GetParseError_En(parse_done.Code()));
	}
}


rapidjson::Document to_json(const std::string& str) {
	rapidjson::Document doc;
	json_load(doc, str);
	return doc;
}


void set_data(Xapian::Document& doc, const std::string& obj_data_str, const std::string& blob_str) {
	char h = DATABASE_DATA_HEADER_MAGIC;
	char f = DATABASE_DATA_FOOTER_MAGIC;
	doc.set_data(std::string(&h, 1) + serialise_length(obj_data_str.size()) + obj_data_str + std::string(&f, 1) + blob_str);
}


MsgPack get_MsgPack(const Xapian::Document& doc) {
	std::string data = doc.get_data();

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) return MsgPack();
	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return MsgPack();
	}
	if (*(p + length) != DATABASE_DATA_FOOTER_MAGIC) return MsgPack();
	return MsgPack(std::string(p, length));
}


std::string get_blob(const Xapian::Document& doc) {
	std::string data = doc.get_data();

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) return data;
	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return data;
	}
	p += length;
	if (*p++ != DATABASE_DATA_FOOTER_MAGIC) return data;
	return std::string(p, p_end - p);
}


std::string to_query_string(std::string str) {
	// '-'' in not accepted by the field processors.
	if (str.at(0) == '-') {
		str[0] = '_';
	}
	return str;
}


std::string msgpack_to_html(const msgpack::object& o) {
	if (o.type == msgpack::type::MAP) {
		std::string key_tag_head = "<dt>";
		std::string key_tag_tail = "</dt>";

		std::string html = "<dl>";
		const msgpack::object_kv* pend(o.via.map.ptr + o.via.map.size);
		for (auto p = o.via.map.ptr; p != pend; ++p) {
			if (p->key.type == msgpack::type::STR) { /* check if it is valid numeric as a key */
				html += key_tag_head + std::string(p->key.via.str.ptr, p->key.via.str.size) + key_tag_tail;
				html += msgpack_map_value_to_html(p->val);
			} else if (p->key.type == msgpack::type::POSITIVE_INTEGER) {
				html += key_tag_head + std::to_string(p->key.via.u64) + key_tag_tail;
				html += msgpack_map_value_to_html(p->val);
			} else if (p->key.type == msgpack::type::NEGATIVE_INTEGER) {
				html += key_tag_head + std::to_string(p->key.via.i64) + key_tag_tail;
				html += msgpack_map_value_to_html(p->val);
			} else if (p->key.type == msgpack::type::FLOAT) {
				html += key_tag_head + std::to_string(p->key.via.f64) + key_tag_tail;
				html += msgpack_map_value_to_html(p->val);
			}
			 /* other types are ignored (boolean included)*/
		}
		html += "</dl>";
		return html;
	} else if (o.type == msgpack::type::ARRAY) {
		std::string term_tag_head = "<li>";
		std::string term_tag_tail = "</li>";

		std::string html = "<ol>";
		const msgpack::object* pend(o.via.array.ptr + o.via.array.size);
		for (auto p = o.via.array.ptr; p != pend; ++p) {
			if (p->type == msgpack::type::STR) {
				html += term_tag_head + std::string(p->via.str.ptr, p->via.str.size) + term_tag_tail;
			} else if(p->type == msgpack::type::POSITIVE_INTEGER) {
				html += term_tag_head + std::to_string(p->via.u64) + term_tag_tail;
			} else if (p->type == msgpack::type::NEGATIVE_INTEGER) {
				html += term_tag_head + std::to_string(p->via.i64) + term_tag_tail;
			} else if (p->type == msgpack::type::FLOAT) {
				html += term_tag_head + std::to_string(p->via.f64) + term_tag_tail;
			} else if (p->type == msgpack::type::BOOLEAN) {
				std::string boolean_str;
				if (p->via.boolean) {
					boolean_str = "True";
				} else {
					boolean_str = "False";
				}
				html += term_tag_head + boolean_str + term_tag_head;
			} else if (p->type == msgpack::type::MAP or p->type == msgpack::type::ARRAY) {
				html += term_tag_head + msgpack_to_html(*p) + term_tag_head;
			}
		}
		html += "</ol>";
	} else if (o.type == msgpack::type::STR) {
		return std::string(o.via.str.ptr, o.via.str.size);
	} else if (o.type == msgpack::type::POSITIVE_INTEGER) {
		return std::to_string(o.via.u64);
	} else if (o.type == msgpack::type::NEGATIVE_INTEGER) {
		return std::to_string(o.via.i64);
	} else if (o.type == msgpack::type::FLOAT) {
		return std::to_string(o.via.f64);
	} else if (o.type == msgpack::type::BOOLEAN) {
		return o.via.boolean ? "True" : "False";
	}

	return std::string();
}


std::string msgpack_map_value_to_html(const msgpack::object& o) {
	std::string tag_head = "<dd>";
	std::string tag_tail = "</dd>";

	if (o.type == msgpack::type::STR) {
		return tag_head + std::string(o.via.str.ptr, o.via.str.size) + tag_tail;
	} else if (o.type == msgpack::type::POSITIVE_INTEGER) {
		return tag_head + std::to_string(o.via.u64) + tag_tail;
	} else if (o.type == msgpack::type::NEGATIVE_INTEGER) {
		return tag_head + std::to_string(o.via.i64) + tag_tail;
	} else if (o.type == msgpack::type::FLOAT) {
		return tag_head + std::to_string(o.via.f64) + tag_tail;
	} else if (o.type == msgpack::type::BOOLEAN) {
		return o.via.boolean ? tag_head + "True" +  tag_tail : tag_head + "False" + tag_tail;
	} else if (o.type == msgpack::type::MAP or o.type == msgpack::type::ARRAY) {
		return tag_head + msgpack_to_html(o) + tag_tail;
	}

	return std::string();
}


std::string msgpack_to_html_error(const msgpack::object& o) {
	std::string html;
	if (o.type == msgpack::type::MAP) {
		const msgpack::object_kv* pend(o.via.map.ptr + o.via.map.size);
		int c = 0;
		html += "<h1>";
		for (auto p = o.via.map.ptr; p != pend; ++p, ++c) {
			if (p->key.type == msgpack::type::STR) {
				if (p->val.type == msgpack::type::STR) {
					if (c) { html += " - "; }
					html += std::string(p->val.via.str.ptr, p->val.via.str.size);
				} else if (p->val.type == msgpack::type::POSITIVE_INTEGER) {
					if (c) { html += " - "; }
					html += std::to_string(p->val.via.u64);
				} else if (p->val.type == msgpack::type::NEGATIVE_INTEGER) {
					if (c) { html += " - "; }
					html += std::to_string(p->val.via.i64);
				} else if (p->val.type == msgpack::type::FLOAT) {
					if (c) { html += " - "; }
					html += std::to_string(p->val.via.f64);
				}
			}
		}
		html += "</h1>";
	}

	return html;
}


Xapian::docid
Indexer::index(Endpoints endpoints, int flags, const MsgPack& obj, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(nullptr, "Database::index(2)");

	L_DATABASE_WRAP(this, "Document to index: %s", obj.to_string().c_str());
	Xapian::Document doc;
	std::string term_id;

	std::shared_ptr<const Schema> schema = XapiandManager::manager->database_pool.get_schema(endpoints[0], flags);
	auto schema_copy = new Schema (*schema);
	
	if (obj.get_type() == msgpack::type::MAP) {
		_index(schema_copy, doc, obj, term_id, _document_id, ct_type, ct_length);
	}

	set_data(doc, obj.to_string(), "");
	L_DATABASE(this, "Schema: %s", schema.to_json_string().c_str());
	std::shared_ptr<Database> database;
	XapiandManager::manager->manager->database_pool.checkout(database, endpoints, flags);
	Xapian::docid did = database->replace_document_term(term_id, doc, commit_);
	XapiandManager::manager->manager->database_pool.checkin(database);
	XapiandManager::manager->database_pool.set_schema(endpoints[0], flags, std::shared_ptr<const Schema>(schema_copy));
	return did;
}


Xapian::docid
Indexer::index(Endpoints endpoints, int flags, const std::string &body, const std::string &_document_id, bool commit_, const std::string &ct_type, const std::string &ct_length) {
	L_CALL(nullptr, "Database::index(1)");

	if (!(flags & DB_WRITABLE)) {
		throw MSG_Error("Database is read-only");
	}

	if (_document_id.empty()) {
		throw MSG_Error("Document must have an 'id'");
	}

	// Create MsgPack object
	bool blob = true;
	std::string ct_type_ = ct_type;
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

	L_DATABASE_WRAP(this, "Document to index: %s", body.c_str());
	Xapian::Document doc;
	std::string term_id;

	if (!endpoints.size()) {
		MSG_Error("Expected exactly one enpoint");
	}

	std::shared_ptr<const Schema> schema = XapiandManager::manager->database_pool.get_schema(endpoints[0], flags);
	auto schema_copy = new Schema (*schema);

	if (obj.get_type() == msgpack::type::MAP) {
		blob = false;
		_index(schema_copy, doc, obj, term_id, _document_id, ct_type_, ct_length);
	}

	set_data(doc, obj.to_string(), blob ? body : "");
	L_DATABASE(this, "Schema: %s", clon_schema.to_json_string().c_str());

	std::shared_ptr<Database> database;
	if (!XapiandManager::manager->manager->database_pool.checkout(database, endpoints, flags)) {
		throw MSG_CheckoutError("Cannot checkout database: %s", endpoints.as_string().c_str());
	}

	Xapian::docid did = database->replace_document_term(term_id, doc, commit_);
	XapiandManager::manager->manager->database_pool.checkin(database);
	XapiandManager::manager->database_pool.set_schema(endpoints[0], flags, std::shared_ptr<const Schema>(schema_copy));
	return did;
}


void
Indexer::_index(Schema* schema, Xapian::Document& doc, const MsgPack& obj, std::string& term_id, const std::string& _document_id, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(nullptr, "Database::_index()");

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
	L_DATABASE_WRAP(this, "Slot: %d _id: %s (%s)", DB_SLOT_ID, _document_id.c_str(), term_id.c_str());

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
		TaskVector tasks;
		tasks.reserve(obj.size());
		for (const auto item_key : obj) {
			const auto str_key = item_key.get_str();
			try {
				auto func = map_dispatch_reserved.at(str_key);
				(*schema.*func)(properties, obj.at(str_key), specification);
			} catch (const std::out_of_range&) {
				if (is_valid(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, &*schema, std::ref(properties), obj.at(str_key), std::ref(specification), std::ref(doc), std::move(str_key)));
				} else {
					try {
						auto func = map_dispatch_root.at(str_key);
						tasks.push_back(std::async(std::launch::deferred, func, &*schema, std::ref(properties), obj.at(str_key), std::ref(specification), std::ref(doc)));
					} catch (const std::out_of_range&) { }
				}
			}
		}

		schema->restart_specification(specification);
		const specification_t spc_start = specification;
		for (auto& task : tasks) {
			task.get();
			specification = spc_start;
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
Indexer::patch(Endpoints endpoints, int flags, const std::string& patches, const std::string& _document_id, bool commit_, const std::string& ct_type, const std::string& ct_length)
{
	L_CALL(this, "Database::patch()");

	if (!(flags & DB_WRITABLE)) {
		throw MSG_Error("database is read-only");
	}

	if (_document_id.empty()) {
		throw MSG_ClientError("Document must have an 'id'");
	}

	rapidjson::Document rdoc_patch;
	MIMEType t = get_mimetype(ct_type);
	MsgPack obj_patch;
	std::string _ct_type(ct_type);
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

	Xapian::QueryParser queryparser;
	queryparser.add_boolean_prefix(RESERVED_ID, prefix);
	auto query = queryparser.parse_query(std::string(RESERVED_ID) + ":" + _document_id);

	std::shared_ptr<Database> database;
	if (!XapiandManager::manager->database_pool.checkout(database, endpoints, flags)) {
		throw MSG_CheckoutError("Cannot checkout database: %s", endpoints.as_string().c_str());
	}

	Xapian::Enquire enquire(*database->db);
	enquire.set_query(query);
	Xapian::MSet mset = enquire.get_mset(0, 1);

	if (mset.empty()) {
		throw MSG_DocNotFoundError("Document not found");
	}
	Xapian::Document document = database->get_document(*mset.begin());

	XapiandManager::manager->database_pool.checkin(database);

	MsgPack obj_data = get_MsgPack(document);
	apply_patch(obj_patch, obj_data);

	L_DATABASE_WRAP(this, "Document to index: %s", obj_data.to_json_string().c_str());

	std::shared_ptr<const Schema> schema = XapiandManager::manager->database_pool.get_schema(endpoints[0]);
	auto schema_copy = new Schema (*schema);

	Xapian::Document doc;
	std::string term_id;
	Indexer::_index(schema_copy, doc, obj_data, term_id, _document_id, _ct_type, ct_length);

	set_data(doc, obj_data.to_string(), get_blob(document));
	L_DATABASE(this, "Schema: %s", schema.to_json_string().c_str());

	if (!XapiandManager::manager->database_pool.checkout(database, endpoints, flags)) {
		throw MSG_CheckoutError("Cannot checkout database: %s", endpoints.as_string().c_str());
	}

	int did = database->replace_document_term(term_id, doc, commit_);
	XapiandManager::manager->manager->database_pool.checkin(database);
	XapiandManager::manager->database_pool.set_schema(endpoints[0], std::shared_ptr<const Schema>(schema_copy));
	return did;
}
