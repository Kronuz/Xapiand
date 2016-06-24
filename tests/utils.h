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

#include "../src/database_handler.h"
#include "../src/endpoint.h"
#include "../src/log.h"
#include "../src/manager.h"
#include "../src/utils.h"
#include "../src/xapiand.h"

#include <fstream>
#include <sstream>

#define TEST_VERBOSITY 3
#define TEST_DETACH false
#define TEST_CHERT false
#define TEST_SOLO true
#define TEST_DATABASE ""
#define TEST_CLUSTER_NAME "cluster_test"
#define TEST_NODE_NAME "node_test"
#define TEST_PIDFILE ""
#define TEST_LOGFILE ""
#define TEST_UID ""
#define TEST_GID ""
#define TEST_DISCOVERY_GROUP ""
#define TEST_RAFT_GROUP ""
#define TEST_NUM_SERVERS 1
#define TEST_DBPOOL_SIZE 1
#define TEST_NUM_REPLICATORS 1
#define TEST_THREADPOOL_SIZE 1
#define TEST_ENDPOINT_LIST_SIZE 1
#define TEST_NUM_COMMITERS 1
#define TEST_EV_FLAG 0
#define TEST_LOCAL_HOST "127.0.0.1"


#define RETURN(x) { Log::finish(); return x; }


inline bool write_file_contents(const std::string& filename, const std::string& contents) {
	std::ofstream of(filename.data(), std::ios::out | std::ios::binary);
	if (of.bad()) {
		return false;
	}
	of.write(contents.data(), contents.size());
	return true;
}


inline bool read_file_contents(const std::string& filename, std::string* contents) {
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


inline bool build_path(const std::string& path) {
	std::string dir = path;
	std::size_t found = dir.find_last_of("/\\");
	dir.resize(found);
	if (exist(dir)) {
		return true;
	} else {
		std::vector<std::string> directories;
		stringTokenizer(dir, "/", directories);
		dir.clear();
		for (const auto& _dir : directories) {
			dir.append(_dir).append(1, '/');
			if (mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
				return false;
			}
		}
		return true;
	}
}


/*
 *	The database used in the test is local
 *	so the Endpoints and local_node are manipulated.
 */

inline Endpoint create_endpoint(const std::string& database) {
	Endpoint e(database, nullptr, -1, TEST_NODE_NAME);
	e.port = XAPIAND_BINARY_SERVERPORT;
	e.host.assign(TEST_LOCAL_HOST);
	return e;
}


struct DB_Test {
	DatabaseHandler db_handler;
	std::string name_database;
	Endpoints endpoints;

	DB_Test(const std::string& db_name, const std::vector<std::string>& docs, int flags)
		: name_database(db_name)
	{
		// Delete database to create.
		delete_files(name_database);
		create_manager();

		endpoints.add(create_endpoint(name_database));

		db_handler.reset(endpoints, flags);

		// Index documents in the database.
		size_t i = 1;
		for (const auto& doc : docs) {
			std::string buffer;
			if (!read_file_contents(doc, &buffer)) {
				delete_files(name_database);
				L_ERR(nullptr, "Can not read the file %s", doc.c_str());
			} else if (db_handler.index(buffer, std::to_string(i++), true, JSON_TYPE, std::to_string(buffer.size())) == 0) {
				delete_files(name_database);
				throw MSG_Error("File %s can not index", doc.c_str());
			}
		}
	}

	~DB_Test() {
		XapiandManager::manager.reset();
		delete_files(name_database);
	}

	void create_manager() {
		if (!XapiandManager::manager) {
			opts_t opts = {
				TEST_VERBOSITY, TEST_DETACH, TEST_CHERT, TEST_SOLO, TEST_DATABASE,
				TEST_CLUSTER_NAME, TEST_NODE_NAME, XAPIAND_HTTP_SERVERPORT, XAPIAND_BINARY_SERVERPORT,
				XAPIAND_DISCOVERY_SERVERPORT, XAPIAND_RAFT_SERVERPORT, TEST_PIDFILE,
				TEST_LOGFILE, TEST_UID, TEST_GID, TEST_DISCOVERY_GROUP, TEST_RAFT_GROUP,
				TEST_NUM_SERVERS, TEST_DBPOOL_SIZE, TEST_NUM_REPLICATORS, TEST_THREADPOOL_SIZE,
				TEST_ENDPOINT_LIST_SIZE, TEST_NUM_COMMITERS, TEST_EV_FLAG
			};

			ev::default_loop default_loop(opts.ev_flags);
			XapiandManager::manager = Worker::make_shared<XapiandManager>(&default_loop, opts.ev_flags, opts);
		}
	}
};
