/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "io_utils.h"
#include "length.h"
#include "log.h"
#include "serialise.h"
#include "utils.h"

#include "rapidjson/error/en.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>


const std::regex find_types_re("(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" FLOAT_STR "|" INTEGER_STR "|" POSITIVE_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")", std::regex::icase | std::regex::optimize);


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
			sep_types[2] = toupper(m.str(3).at(0));
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
	if (document.is_map()) {
		bool obj = false;
		MsgPack _val;
		for (auto item_key = document.begin(); item_key != document.end();) {
			auto str_key((*item_key).as_string());
			if (is_valid(str_key)) {
				obj = true;
				auto& item_doc = document.at(str_key);
				clean_reserved(item_doc);
				++item_key;
			} else if (str_key == RESERVED_VALUE) {
				_val = document.at(str_key);
				++item_key;
			} else {
				item_key = document.erase(item_key);
			}
		}
		if (!obj) {
			document = _val;
		}
	}
}


MIMEType get_mimetype(const std::string& type) {
	if (type == JSON_CONTENT_TYPE) {
		return MIMEType::APPLICATION_JSON;
	} else if (type == FORM_URLENCODED_CONTENT_TYPE) {
		return MIMEType::APPLICATION_XWWW_FORM_URLENCODED;
	} else if (type == MSGPACK_CONTENT_TYPE) {
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


MsgPack get_MsgPack(const Xapian::Document& doc) {
	auto data = doc.get_data();

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) {
		return MsgPack();
	}

	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return MsgPack();
	}

	if (*(p + length) != DATABASE_DATA_FOOTER_MAGIC) {
		return MsgPack();
	}

	return MsgPack::unserialise(std::string(p, length));
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
