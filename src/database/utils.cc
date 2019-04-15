/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "database/utils.h"

#include <algorithm>                                 // for count, replace
#include <chrono>                                    // for seconds, duration_cast
#include <cstdio>                                    // for snprintf, size_t
#include <cstring>                                   // for strlen
#include <fcntl.h>                                   // for O_CLOEXEC, O_CREAT, O_RDONLY
#include <sys/stat.h>                                // for stat

#include "base_x.hh"                                 // for base62
#include "cast.h"                                    // for Cast
#include "database/schema.h"                         // for FieldType
#include "exception.h"                               // for ClientError, MSG_ClientError
#include "io.hh"                                     // for close, open, read, write
#include "length.h"                                  // for serialise_length and unserialise_length
#include "log.h"                                     // for L_DATABASE
#include "opts.h"                                    // for opts
#include "rapidjson/document.h"                      // for Document, GenericDocument
#include "rapidjson/error/en.h"                      // for GetParseError_En
#include "rapidjson/error/error.h"                   // for ParseResult
#include "serialise.h"                               // for Serialise
#include "storage.h"                                 // for STORAGE_BIN_HEADER_MAGIC and STORAGE_BIN_FOOTER_MAGIC
#include "y2j/y2j.h"                                 // for y2j::yamlParseBytes


std::string prefixed(std::string_view term, std::string_view field_prefix, char field_type)
{
	std::string result;
	result.reserve(field_prefix.size() + term.size() + 1);
	result.assign(field_prefix).push_back(field_type);
	result.append(term);
	return result;
}


Xapian::valueno get_slot(std::string_view field_prefix, char field_type)
{
	auto slot = static_cast<Xapian::valueno>(xxh32::hash(std::string(field_prefix) + field_type));
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


std::string get_prefix(std::string_view field_name)
{
	// Mask 0x1fffff for maximum length prefix of 4.
	return serialise_length(xxh32::hash(field_name) & 0x1fffff);
}


std::string normalize_uuid(std::string_view uuid)
{
	return Unserialise::uuid(Serialise::uuid(uuid), static_cast<UUIDRepr>(opts.uuid_repr));
}


std::string normalize_uuid(const std::string& uuid) {
	return normalize_uuid(std::string_view(uuid));
}


MsgPack normalize_uuid(const MsgPack& uuid)
{
	if (uuid.is_string()) {
		return normalize_uuid(uuid.str_view());
	}
	return uuid;
}


int read_uuid(std::string_view dir, std::array<unsigned char, 16>& uuid)
{
	auto sdir = std::string(dir);
	L_DATABASE("+ READING UUID OF INDEX '{}'...", sdir);

	struct stat info;
	if ((::stat(sdir.c_str(), &info) != 0) || ((info.st_mode & S_IFDIR) == 0)) {
		L_DATABASE("- NO DATABASE INDEX '{}'", sdir);
		return -1;
	}

	int fd = io::open((sdir + "/iamglass").c_str(), O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		L_DATABASE("- NO DATABASE INDEX '{}'", sdir);
		return -1;
	}

	char bytes[32];
	size_t length = io::read(fd, bytes, 32);
	io::close(fd);
	if (length == 32) {
		std::copy(bytes + 16, bytes + 32, uuid.begin());
		return 0;
	}

	return -1;
}


void json_load(rapidjson::Document& doc, std::string_view str)
{
	rapidjson::ParseResult parse_done = doc.Parse(str.data(), str.size());
	if (!parse_done) {
		constexpr size_t tabsize = 3;
		std::string tabs(tabsize, ' ');
		auto offset = parse_done.Offset();
		char buffer[22];
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
		std::string snippet(buffer);
		auto indent = sz + snippet.size();
		snippet.append(a);
		snippet.append(b);
		snippet.push_back('\n');
		snippet.append(indent, ' ');
		snippet.push_back('^');
		size_t p = 0;
		while ((p = snippet.find("\t", p)) != std::string::npos) {
			snippet.replace(p, 1, tabs);
			++p;
		}
		THROW(ClientError, "JSON parse error at line {}, col: {} : {}\n{}", line, col, GetParseError_En(parse_done.Code()), snippet);
	}
}


void yaml_load(rapidjson::Document& doc, std::string_view str)
{
	const char* errorMessage = nullptr;
	size_t errorLine = 0;
	doc = y2j::yamlParseBytes(str.data(), str.size(), &errorMessage, &errorLine);
	if (errorMessage) {
		THROW(ClientError, "JSON parse error at line {} : {}", errorLine, errorMessage);
	}
}


rapidjson::Document to_json(std::string_view str)
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
	}

	if (o.type == msgpack::type::ARRAY) {
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
		return html;
	}

	if (o.type == msgpack::type::STR) {
		return std::string(o.via.str.ptr, o.via.str.size);
	}

	if (o.type == msgpack::type::POSITIVE_INTEGER) {
		return std::to_string(o.via.u64);
	}

	if (o.type == msgpack::type::NEGATIVE_INTEGER) {
		return std::to_string(o.via.i64);
	}

	if (o.type == msgpack::type::FLOAT) {
		return std::to_string(o.via.f64);
	}

	if (o.type == msgpack::type::BOOLEAN) {
		return o.via.boolean ? "True" : "False";
	}

	return "";
}


std::string msgpack_map_value_to_html(const msgpack::object& o)
{
	std::string tag_head = "<dd>";
	std::string tag_tail = "</dd>";

	if (o.type == msgpack::type::STR) {
		return tag_head + std::string(o.via.str.ptr, o.via.str.size) + tag_tail;
	}

	if (o.type == msgpack::type::POSITIVE_INTEGER) {
		return tag_head + std::to_string(o.via.u64) + tag_tail;
	}

	if (o.type == msgpack::type::NEGATIVE_INTEGER) {
		return tag_head + std::to_string(o.via.i64) + tag_tail;
	}

	if (o.type == msgpack::type::FLOAT) {
		return tag_head + std::to_string(o.via.f64) + tag_tail;
	}

	if (o.type == msgpack::type::BOOLEAN) {
		return o.via.boolean ? tag_head + "True" +  tag_tail : tag_head + "False" + tag_tail;
	}

	if (o.type == msgpack::type::MAP or o.type == msgpack::type::ARRAY) {
		return tag_head + msgpack_to_html(o) + tag_tail;
	}

	return "";
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
					if (c != 0) { html += " - "; }
					html += std::string(p->val.via.str.ptr, p->val.via.str.size);
				} else if (p->val.type == msgpack::type::POSITIVE_INTEGER) {
					if (c != 0) { html += " - "; }
					html += std::to_string(p->val.via.u64);
				} else if (p->val.type == msgpack::type::NEGATIVE_INTEGER) {
					if (c != 0) { html += " - "; }
					html += std::to_string(p->val.via.i64);
				} else if (p->val.type == msgpack::type::FLOAT) {
					if (c != 0) { html += " - "; }
					html += std::to_string(p->val.via.f64);
				}
			}
		}
		html += "</h1>";
	}

	return html;
}


void split_path_id(std::string_view path_id, std::string_view& path, std::string_view& id)
{
	std::size_t found = path_id.find_last_of('/');
	if (found != std::string::npos) {
		path = path_id.substr(0, found);
		id = path_id.substr(found + 1);
	} else {
		path = "";
		id = "";
	}
}
