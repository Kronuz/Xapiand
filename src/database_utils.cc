/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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

#include <algorithm>                                 // for count, replace
#include <chrono>                                    // for seconds, duration_cast
#include <ratio>                                     // for ratio
#include <stdio.h>                                   // for snprintf, size_t
#include <string.h>                                  // for strlen
#include <sys/fcntl.h>                               // for O_CLOEXEC, O_CREAT, O_RDONLY
#include <sys/stat.h>                                // for stat

#include "base_x.hh"                                 // for base62
#include "cast.h"                                    // for Cast
#include "exception.h"                               // for ClientError, MSG_ClientError
#include "io_utils.h"                                // for close, open, read, write
#include "length.h"                                  // for serialise_length and unserialise_length
#include "log.h"                                     // for L_DATABASE
#include "opts.h"                                    // for opts
#include "rapidjson/document.h"                      // for Document, GenericDocument
#include "rapidjson/error/en.h"                      // for GetParseError_En
#include "rapidjson/error/error.h"                   // for ParseResult
#include "schema.h"                                  // for FieldType
#include "serialise.h"                               // for Serialise
#include "storage.h"                                 // for STORAGE_BIN_HEADER_MAGIC and STORAGE_BIN_FOOTER_MAGIC
#include "utils.h"                                   // for random_int


inline static long long save_mastery(const std::string& dir)
{
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


std::string prefixed(const std::string& term, const std::string& field_prefix, char field_type)
{
	std::string result;
	result.reserve(field_prefix.length() + term.length() + 1);
	result.assign(field_prefix).push_back(field_type);
	result.append(term);
	return result;
}


Xapian::valueno get_slot(const std::string& field_prefix, char field_type)
{
	auto slot = static_cast<Xapian::valueno>(xxh64::hash(field_prefix + field_type));
	if (slot < DB_SLOT_RESERVED) {
		slot += DB_SLOT_RESERVED;
	} else if (slot == Xapian::BAD_VALUENO) {
		slot = 0xfffffffe;
	}
	return slot;
}


std::string get_prefix(unsigned long long field_number)
{
	return serialise_length(field_number);
}


std::string get_prefix(const std::string& field_name)
{
	// Mask 0x1fffff for maximum length prefix of 4.
	return serialise_length(xxh64::hash(field_name) & 0x1fffff);
}


std::string normalize_uuid(const std::string& uuid)
{
	return Unserialise::uuid(Serialise::uuid(uuid), static_cast<UUIDRepr>(opts.uuid_repr));
}


MsgPack normalize_uuid(const MsgPack& uuid)
{
	if (uuid.is_string()) {
		return normalize_uuid(uuid.str());
	}
	return uuid;
}


long long read_mastery(const std::string& dir, bool force)
{
	L_DATABASE("+ READING MASTERY OF INDEX '%s'...", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		L_DATABASE("- NO MASTERY OF INDEX '%s'", dir.c_str());
		return -1;
	}

	long long mastery_level = -1;

	int fd = io::open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC);
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

	L_DATABASE("- MASTERY OF INDEX '%s' is %llx", dir.c_str(), mastery_level);

	return mastery_level;
}


void json_load(rapidjson::Document& doc, const std::string& str)
{
	rapidjson::ParseResult parse_done = doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(str.data());
	if (!parse_done) {
		constexpr size_t tabsize = 3;
		auto offset = parse_done.Offset();
		char buffer[20];
		auto a = str.substr(0, offset);
		auto line = std::count(a.begin(), a.end(), '\n') + 1;
		if (line > 1) {
			auto f = a.rfind("\n");
			if (f != std::string::npos) {
				a = a.substr(f + 1);
			}
		}
		snprintf(buffer, sizeof(buffer), "%zu. ", line);
		auto b = str.substr(offset);
		b = b.substr(0, b.find("\n"));
		auto tsz = std::count(a.begin(), a.end(), '\t');
		auto col = a.size() + 1;
		auto sz = col - 1 - tsz + tsz * tabsize;
		auto snippet = std::string(buffer) + a + b + "\n" + std::string(sz + strlen(buffer), ' ') + '^';
		size_t p = 0;
		while ((p = snippet.find("\t", p)) != std::string::npos) {
			snippet.replace(p, 1, std::string(tabsize, ' '));
			++p;
		}
		THROW(ClientError, "JSON parse error at line %zu, col: %zu : %s\n%s", line, col, GetParseError_En(parse_done.Code()), snippet.c_str());
	}
}


rapidjson::Document to_json(const std::string& str)
{
	rapidjson::Document doc;
	json_load(doc, str);
	return doc;
}


std::string msgpack_to_html(const msgpack::object& o)
{
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


std::string msgpack_map_value_to_html(const msgpack::object& o)
{
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


std::string msgpack_to_html_error(const msgpack::object& o)
{
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


std::string join_data(bool stored, const std::string& stored_locator, const std::string& obj, const std::string& blob)
{
	/* Joined data is as follows:
	 * For stored (in storage) blobs:
	 *   In any case, when object gets saved, and if available, it uses
	 *   <blob> to update (or create) a valid locator and save the data.
	 *       1. <stored_locator> is a valid locator, and there may not be a <blob>.
	 *       2. <stored_locator> is empty, and there is a <blob>
	 *   "<DATA_HEADER_MAGIC_STORED><stored_locator><object><DATA_FOOTER_MAGIC><blob>"
	 * For inplace (not stored) blobs:
	 *   "<DATA_HEADER_MAGIC><object><DATA_FOOTER_MAGIC><blob>"
	 */
	L_CALL("::join_data(<stored>, <stored_locator>, <obj>, <blob>)");

	auto obj_len = serialise_length(obj.size());
	std::string data;
	if (stored) {
		auto stored_locator_len = serialise_length(stored_locator.size());
		data.reserve(1 + obj_len.size() + obj.size() + stored_locator_len.size() + stored_locator.size() + 1 + blob.size());
		data.push_back(DATABASE_DATA_HEADER_MAGIC_STORED);
		data.append(stored_locator_len);
		data.append(stored_locator);
	} else {
		data.reserve(1 + obj_len.size() + obj.size() + 1 + blob.size());
		data.push_back(DATABASE_DATA_HEADER_MAGIC);
	}
	data.append(obj_len);
	data.append(obj);
	data.push_back(DATABASE_DATA_FOOTER_MAGIC);
	data.append(blob);
	return data;
}


std::pair<bool, std::string> split_data_store(const std::string& data)
{
	L_CALL("::split_data_store(<data>)");

	std::string stored_locator;
	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		return std::make_pair(false, std::string());
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (const Xapian::SerialisationError&) {
			return std::make_pair(false, std::string());
		}
		stored_locator = std::string(p, length);
		p += length;
	} else {
		return std::make_pair(false, std::string());
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return std::make_pair(false, std::string());
	}

	if (*(p + length) == DATABASE_DATA_FOOTER_MAGIC) {
		return std::make_pair(true, stored_locator);
	}

	return std::make_pair(false, std::string());
}


std::string split_data_obj(const std::string& data)
{
	L_CALL("::split_data_obj(<data>)");

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		++p;
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (const Xapian::SerialisationError&) {
			return std::string();
		}
		p += length;
	} else {
		return std::string();
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (const Xapian::SerialisationError&) {
		return std::string();
	}

	if (*(p + length) == DATABASE_DATA_FOOTER_MAGIC) {
		return std::string(p, length);
	}

	return std::string();
}


std::string get_data_content_type(const std::string& data)
{
	L_CALL("::get_data_content_type(<data>)");

	std::string stored_locator;
	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		++p;
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (const Xapian::SerialisationError&) {
			return "";
		}
		stored_locator = std::string(p, length);
		p += length;
	} else {
		return "";
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (const Xapian::SerialisationError&) {
		return "";
	}
	p += length;

	if (*p == DATABASE_DATA_FOOTER_MAGIC) {
		++p;
		if (!stored_locator.empty()) {
			return std::get<3>(storage_unserialise_locator(stored_locator));
		} else if (p != p_end) {
			return unserialise_string_at(STORED_BLOB_CONTENT_TYPE, &p, p_end);
		} else {
			return "";
		}
	}

	return "";
}


std::string split_data_blob(const std::string& data)
{
	L_CALL("::split_data_blob(<data>)");

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p == DATABASE_DATA_HEADER_MAGIC) {
		++p;
	} else if (*p == DATABASE_DATA_HEADER_MAGIC_STORED) {
		++p;
		try {
			length = unserialise_length(&p, p_end, true);
		} catch (const Xapian::SerialisationError&) {
			return data;
		}
		p += length;
	} else {
		return data;
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (const Xapian::SerialisationError&) {
		return data;
	}
	p += length;

	if (*p == DATABASE_DATA_FOOTER_MAGIC) {
		++p;
		return std::string(p, p_end - p);
	}

	return data;
}


void split_path_id(const std::string& path_id, std::string& path, std::string& id)
{
	std::size_t found = path_id.find_last_of('/');
	if (found != std::string::npos) {
		path = path_id.substr(0, found);
		id = path_id.substr(found + 1);
	} else {
		path.empty();
		id.empty();
	}
}


#ifdef XAPIAND_DATA_STORAGE
std::tuple<ssize_t, size_t, size_t, std::string> storage_unserialise_locator(const std::string& store)
{
	ssize_t volume;
	size_t offset, size;
	std::string content_type;

	const char *p = store.data();
	const char *p_end = p + store.size();
	if (*p++ != STORAGE_BIN_HEADER_MAGIC) {
		return std::make_tuple(-1, 0, 0, "");
	}
	try {
		volume = unserialise_length(&p, p_end);
	} catch (const Xapian::SerialisationError&) {
		return std::make_tuple(-1, 0, 0, "");
	}
	try {
		offset = unserialise_length(&p, p_end);
	} catch (const Xapian::SerialisationError&) {
		return std::make_tuple(-1, 0, 0, "");
	}
	try {
		size = unserialise_length(&p, p_end);
	} catch (const Xapian::SerialisationError&) {
		return std::make_tuple(-1, 0, 0, "");
	}
	try {
		content_type = unserialise_string(&p, p_end);
	} catch (const Xapian::SerialisationError&) {
		return std::make_tuple(-1, 0, 0, "");
	}
	if (*p++ != STORAGE_BIN_FOOTER_MAGIC) {
		return std::make_tuple(-1, 0, 0, "");
	}

	return std::make_tuple(volume, offset, size, content_type);
}


std::string storage_serialise_locator(ssize_t volume, size_t offset, size_t size, const std::string& content_type)
{
	std::string ret;
	ret.append(1, STORAGE_BIN_HEADER_MAGIC);
	ret.append(serialise_length(volume));
	ret.append(serialise_length(offset));
	ret.append(serialise_length(size));
	ret.append(serialise_string(content_type));
	ret.append(1, STORAGE_BIN_FOOTER_MAGIC);
	return ret;
}
#endif /* XAPIAND_DATA_STORAGE */
