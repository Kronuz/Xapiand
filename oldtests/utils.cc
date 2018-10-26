/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "../src/fs.h"
#include "../src/opts.h"
#include "../src/hashes.hh"                 // for fnv1ah32

opts_t opts;

Initializer::Initializer()
{
	if (!XapiandManager::manager) {
		opts = opts_t{
			/* int verbosity = */ 3,
			/* bool detach = */ false,
			/* bool chert = */ false,
			/* bool solo = */ true,
			/* bool strict = */ false,
			/* bool force = */ false,
			/* bool optimal = */ false,
			/* bool foreign = */ false,
			/* bool colors = */ false,
			/* bool no_colors = */ false,
			/* std::string database = */ "",
			/* std::string cluster_name = */ TEST_CLUSTER_NAME,
			/* std::string node_name = */ TEST_NODE_NAME,
			/* unsigned int http_port = */ XAPIAND_HTTP_SERVERPORT,
			/* unsigned int binary_port = */ XAPIAND_BINARY_SERVERPORT,
			/* unsigned int discovery_port = */ XAPIAND_DISCOVERY_SERVERPORT,
			/* unsigned int raft_port = */ XAPIAND_RAFT_SERVERPORT,
			/* std::string pidfile = */ "",
			/* std::string logfile = */ "",
			/* std::string uid = */ "",
			/* std::string gid = */ "",
			/* std::string discovery_group = */ "",
			/* std::string raft_group = */ "",
			/* ssize_t num_servers = */ 1,
			/* ssize_t dbpool_size = */ 1,
			/* ssize_t num_replicators = */ 1,
			/* ssize_t threadpool_size = */ 1,
			/* ssize_t tasks_size = */ 1,
			/* ssize_t endpoints_list_size = */ 1,
			/* ssize_t num_committers = */ 1,
			/* ssize_t num_fsynchers = */ NUM_FSYNCHERS,
			/* ssize_t max_clients = */ 100,
			/* ssize_t max_databases = */ MAX_DATABASES,
			/* ssize_t max_files = */ 1000,
			/* int flush_threshold = */ FLUSH_THRESHOLD,
			/* unsigned int ev_flags = */ 0,
			/* bool uuid_compact = */ true,
			/* UUIDRepr uuid_repr = */ fnv1ah32::hash("simple"),
			/* bool uuid_partition = */ true,
			/* std::string dump_metadata = */ "",
			/* std::string dump_schema = */ "",
			/* std::string dump_documents = */ "",
			/* std::string restore = */ "",
			/* std::string filename = */ "",
		};

		static ev::default_loop default_loop(opts.ev_flags);
		XapiandManager::manager = Worker::make_shared<XapiandManager>(&default_loop, opts.ev_flags);
	}
}


void Initializer::destroy()
{
	XapiandManager::manager.reset();
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
