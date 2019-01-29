/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "utils.h"

#include "../src/fs.hh"
#include "../src/opts.h"
#include "../src/hashes.hh"                 // for fnv1ah32

opts_t opts;

Initializer::Initializer()
{
	if (!XapiandManager::manager()) {

		// And some defaults for testing:
		opts.verbosity = 3;
		opts.cluster_name = TEST_CLUSTER_NAME;
		opts.node_name = TEST_NODE_NAME;
		opts.solo = true;
		opts.uuid_compact = true;
		opts.uuid_partition = true;
		opts.log_epoch = true;
		opts.log_threads = true;

		static ev::default_loop default_loop(opts.ev_flags);
		XapiandManager::make(&default_loop, opts.ev_flags);
	}
}


void Initializer::destroy()
{
	XapiandManager::reset();
}


bool write_file_contents(const std::string& filename, const std::string& contents) {
	std::ofstream of(filename.data(), std::ios::out | std::ios::binary);
	if (of.bad()) {
		return false;
	}
	of.write(contents.data(), contents.size());
	return true;
}


bool read_file_contents(const std::string& filename, std::string* contents) {
	std::ifstream in(filename.data(), std::ios::in | std::ios::binary);
	if (in.bad()) {
		return false;
	}

	in.seekg(0, std::ios::end);
	contents->resize(static_cast<size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&(*contents)[0], contents->size());
	in.close();
	return true;
}


DB_Test::DB_Test(std::string_view db_name, const std::vector<std::string>& docs, int flags, const std::string_view ct_type)
	: name_database(db_name)
{
	// Delete database to create new db.
	delete_files(name_database);

	endpoints.add(create_endpoint(name_database));

	db_handler.reset(endpoints, flags, HTTP_GET);

	// Index documents in the database.
	size_t i = 1;
	for (const auto& doc : docs) {
		std::string buffer;
		try {
			if (!read_file_contents(doc, &buffer)) {
				destroy();
				L_ERR("Can not read the file %s", doc);
			} else if (db_handler.index(std::to_string(i++), false, get_body(buffer, ct_type).second, true, ct_type_t(ct_type)).first == 0) {
				destroy();
				THROW(Error, "File %s can not index", doc);
			}
		} catch (const std::exception& e) {
			destroy();
			THROW(Error, "File %s can not index [%s]", doc, e.what());
		}
	}
}


DB_Test::~DB_Test()
{
	destroy();
}


void
DB_Test::destroy()
{
	delete_files(name_database);
}


std::pair<std::string_view, MsgPack>
DB_Test::get_body(std::string_view body, std::string_view ct_type)
{
	MsgPack msgpack;
	rapidjson::Document rdoc;

	constexpr static auto _ = phf::make_phf({
		hhl(FORM_URLENCODED_CONTENT_TYPE),
		hhl(JSON_CONTENT_TYPE),
		hhl(MSGPACK_CONTENT_TYPE),
		hhl(X_MSGPACK_CONTENT_TYPE),
	});

	switch (_.fhhl(ct_type)) {
		case _.fhhl(FORM_URLENCODED_CONTENT_TYPE):
			try {
				json_load(rdoc, body);
				msgpack = MsgPack(rdoc);
			} catch (const std::exception&) {
				msgpack = MsgPack(body);
			}
			break;
		case _.fhhl(JSON_CONTENT_TYPE):
			json_load(rdoc, body);
			msgpack = MsgPack(rdoc);
			break;
		case _.fhhl(MSGPACK_CONTENT_TYPE):
		case _.fhhl(X_MSGPACK_CONTENT_TYPE):
			msgpack = MsgPack::unserialise(body);
			break;
		default:
			msgpack = MsgPack(body);
			break;
	}

	return std::make_pair(ct_type, msgpack);
}
