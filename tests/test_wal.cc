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


#include "test_wal.h"

#include "../src/database.h"
#include "../src/endpoint.h"
#include "../src/log.h"


std::shared_ptr<DatabaseQueue>a_queue;
std::shared_ptr<DatabaseQueue>b_queue;
static std::shared_ptr<Database> database;
static std::shared_ptr<Database> res_database;
static std::string test_db(".test_wal.db");
static std::string restored_db(".backup_wal.db");
constexpr int seed = 0;


uint32_t get_checksum(int fd) {
	ssize_t bytes;
	char buf[1024];

	XXH32_state_t* xxhash = XXH32_createState();
	XXH32_reset(xxhash, seed);

	while ((bytes = io::read(fd, buf, sizeof(buf))) > 0) {
		XXH32_update(xxhash, buf, bytes);
	}

	uint32_t checksum = XXH32_digest(xxhash);
	XXH32_freeState(xxhash);
	return checksum;
}


bool dir_compare(const std::string& dir1, const std::string& dir2) {
	bool same_file = true;
	DIR* d1 = opendir(dir1.c_str());
	if (!d1) {
		L_ERR(nullptr, "ERROR: %s", strerror(errno));
		return false;
	}

	DIR* d2 = opendir(dir2.c_str());
	if (!d2) {
		L_ERR(nullptr, "ERROR: %s", strerror(errno));
		return false;
	}

	struct dirent *ent;
	while ((ent = readdir(d1)) != nullptr) {
		if (ent->d_type == DT_REG) {
			std::string dir1_file (dir1 + "/" + std::string(ent->d_name));
			std::string dir2_file (dir2 + "/" + std::string(ent->d_name));

			int fd1 = open(dir1_file.c_str(), O_RDONLY);
			if (-1 == fd1) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", dir1_file.c_str());
				same_file = false;
				break;
			}

			int fd2 = open(dir2_file.c_str(), O_RDONLY);
			if (-1 == fd2) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", dir2_file.c_str());
				same_file = false;
				close(fd1);
				break;
			}

			if (get_checksum(fd1) != get_checksum(fd2)) {
				L_ERR(nullptr, "ERROR: file %s and file %s are not the same\n", std::string(dir1 + "/" + dir1_file).c_str(), std::string(dir2 + "/" + dir2_file).c_str());
				same_file = false;
				close(fd1);
				close(fd2);
				break;
			}
			close(fd1);
			close(fd2);
		}
	}
	closedir(d1);
	closedir(d2);
	return same_file;
}


int create_db() {
	// Delete databases to create.
	delete_files(test_db);
	delete_files(restored_db);

	int num_documents = 1020;

	std::string document("{ \"message\" : \"Hello world\"}");
	local_node.name.assign("node_test");
	local_node.binary_port = XAPIAND_BINARY_SERVERPORT;

	Endpoints endpoints;
	Endpoint e;
	e.node_name.assign("node_test");
	e.port = XAPIAND_BINARY_SERVERPORT;
	e.path.assign(test_db);
	e.host.assign("0.0.0.0");
	endpoints.add(e);

	database = std::make_shared<Database>(a_queue, endpoints, DB_WRITABLE | DB_SPAWN);
	database->index(document, std:: to_string(1), true, JSON_TYPE, std::to_string(document.size()));

	if (copy_file(test_db.c_str(), restored_db.c_str()) == -1) {
		return 1;
	}

	for (int i = 2; i <= num_documents; ++i) {
		database->index(document, std:: to_string(i), true, JSON_TYPE, std::to_string(document.size()));
	}

	if (copy_file(test_db.c_str(), restored_db.c_str(), true, std::string("wal.0")) == -1) {
		return 1;
	}

	if (copy_file(test_db.c_str(), restored_db.c_str(), true, std::string("wal.1012")) == -1) {
		return 1;
	}

	return 0;
}


int restore_database() {
	int result = 0;
	try {
		result = create_db();
		Endpoints endpoints;
		Endpoint e;
		e.node_name.assign("node_test");
		e.port = XAPIAND_BINARY_SERVERPORT;
		e.path.assign(restored_db);
		e.host.assign("0.0.0.0");
		endpoints.add(e);

		res_database = std::make_shared<Database>(b_queue, endpoints, DB_WRITABLE);
		if (not dir_compare(test_db, restored_db)) {
			++result;
		}
	} catch (const Xapian::Error& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_msg().c_str());
		return 1;
	} catch (const std::exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.what());
		return 1;
	}

	// Delete databases created.
	delete_files(test_db);
	delete_files(restored_db);
	return result;
}
