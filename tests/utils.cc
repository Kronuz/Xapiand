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

#include "utils.h"


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


DB_Test::DB_Test(const std::string& db_name, const std::vector<std::string>& docs, int flags, const std::string& ct_type)
	: name_database(db_name)
{
	// Delete database to create.
	delete_files(name_database);
	create_manager();

	endpoints.add(create_endpoint(name_database));

	db_handler.reset(endpoints, flags, HTTP_GET);

	// Index documents in the database.
	size_t i = 1;
	for (const auto& doc : docs) {
		std::string buffer;
		try {
			if (!read_file_contents(doc, &buffer)) {
				delete_files(name_database);
				L_ERR(nullptr, "Can not read the file %s", doc.c_str());
			} else if (db_handler.index(std::to_string(i++), get_body(buffer, ct_type).second, true, ct_type).first == 0) {
				delete_files(name_database);
				THROW(Error, "File %s can not index", doc.c_str());
			}
		} catch (const std::exception& e) {
			THROW(Error, "File %s can not index [%s]", doc.c_str(), e.what());
		}
	}
}


DB_Test::~DB_Test()
{
	delete_files(name_database);
}


void
DB_Test::create_manager()
{
	if (!XapiandManager::manager) {
		opts_t opts = {
			TEST_VERBOSITY, TEST_DETACH, TEST_CHERT, TEST_SOLO, TEST_REQUIRED_TYPE, TEST_DATABASE,
			TEST_CLUSTER_NAME, TEST_NODE_NAME, XAPIAND_HTTP_SERVERPORT, XAPIAND_BINARY_SERVERPORT,
			XAPIAND_DISCOVERY_SERVERPORT, XAPIAND_RAFT_SERVERPORT, TEST_PIDFILE,
			TEST_LOGFILE, TEST_UID, TEST_GID, TEST_DISCOVERY_GROUP, TEST_RAFT_GROUP,
			TEST_NUM_SERVERS, TEST_DBPOOL_SIZE, TEST_NUM_REPLICATORS, TEST_THREADPOOL_SIZE,
			TEST_ENDPOINT_LIST_SIZE, TEST_NUM_COMMITERS, CONFIG_DEFAULT_MAX_CLIENTS, TEST_EV_FLAG
		};

		ev::default_loop default_loop(opts.ev_flags);
		XapiandManager::manager = Worker::make_shared<XapiandManager>(&default_loop, opts.ev_flags, opts);
	}
}


std::pair<std::string, MsgPack>
DB_Test::get_body(const std::string& body, const std::string& ct_type)
{
	MsgPack msgpack;
	rapidjson::Document rdoc;
	switch (xxh64::hash(ct_type)) {
		case xxh64::hash(FORM_URLENCODED_CONTENT_TYPE):
			try {
				json_load(rdoc, body);
				msgpack = MsgPack(rdoc);
			} catch (const std::exception&) {
				msgpack = MsgPack(body);
			}
			break;
		case xxh64::hash(JSON_CONTENT_TYPE):
			json_load(rdoc, body);
			msgpack = MsgPack(rdoc);
			break;
		case xxh64::hash(MSGPACK_CONTENT_TYPE):
		case xxh64::hash(X_MSGPACK_CONTENT_TYPE):
			msgpack = MsgPack::unserialise(body);
			break;
		default:
			msgpack = MsgPack(body);
			break;
	}

	return std::make_pair(ct_type, msgpack);
}