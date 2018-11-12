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

#include "test_wal.h"

#include "../src/database.h"
#include "../src/database_wal.h"


const std::string test_db(".test_wal.db");
const std::string restored_db(".backup_wal.db");


uint32_t get_checksum(int fd) {
	ssize_t bytes;
	char buf[1024];

	XXH32_state_t* xxhash = XXH32_createState();
	XXH32_reset(xxhash, 0);

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
		L_ERR("ERROR: %s", strerror(errno));
		return false;
	}

	DIR* d2 = opendir(dir2.c_str());
	if (!d2) {
		L_ERR("ERROR: %s", strerror(errno));
		return false;
	}

	struct dirent *ent;
	while ((ent = readdir(d1)) != nullptr) {
		if (ent->d_type == DT_REG) {
			std::string dir1_file(dir1 + "/" + std::string(ent->d_name));
			std::string dir2_file(dir2 + "/" + std::string(ent->d_name));

			int fd1 = open(dir1_file.c_str(), O_RDONLY);
			if (-1 == fd1) {
				L_ERR("ERROR: opening file. %s\n", dir1_file);
				same_file = false;
				break;
			}

			int fd2 = open(dir2_file.c_str(), O_RDONLY);
			if (-1 == fd2) {
				L_ERR("ERROR: opening file. %s\n", dir2_file);
				same_file = false;
				close(fd1);
				break;
			}

			if (get_checksum(fd1) != get_checksum(fd2)) {
				L_ERR("ERROR: file %s and file %s are not the same\n", dir1_file, dir2_file);
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


int create_db_wal() {
	static DB_Test db_wal(test_db, std::vector<std::string>(), DB_WRITABLE | DB_CREATE_OR_OPEN);

	int num_documents = 1020;
	std::string document("{ \"message\" : \"Hello world\"}");

	DatabaseWALWriter::start(1);

	auto re = db_wal.get_body(document, JSON_CONTENT_TYPE);

	db_wal.db_handler.index(std::to_string(1), false, re.second, true, json_type);

	if (copy_file(test_db.c_str(), restored_db.c_str()) == -1) {
		L_ERR("ERROR: Could not copy the dir %s to dir %s\n", test_db, restored_db);
		return 1;
	}

	for (int i = 2; i <= num_documents; ++i) {
		db_wal.db_handler.index(std::to_string(i), false, re.second, true, json_type);
	}

	if (copy_file(test_db.c_str(), restored_db.c_str(), true, std::string("wal.0")) == -1) {
		L_ERR("ERROR: Could not copy the dir %s to dir %s\n", "wal.0", restored_db);
		return 1;
	}

	if (copy_file(test_db.c_str(), restored_db.c_str(), true, std::string("wal.1016")) == -1) {
		L_ERR("ERROR: Could not copy the file %s to dir %s\n", "wal.1016", restored_db);
		return 1;
	}

	return 0;
}


int restore_database() {
	INIT_LOG
#if XAPIAND_DATABASE_WAL
	try {
		if (create_db_wal() == 0) {
			/* Trigger the backup wal */
			Endpoints endpoints;
			endpoints.add(create_endpoint(restored_db));
			const auto queue_state(std::make_shared<queue::QueueState>(-1, 1, -1));
			auto b_queue = DatabaseQueue::make_shared(endpoints, queue_state);
			std::shared_ptr<Database> res_database = std::make_shared<Database>(b_queue, DB_WRITABLE);
			if (not dir_compare(test_db, restored_db)) {
				delete_files(restored_db);
				RETURN(1);
			}
			delete_files(restored_db);
			RETURN(0);
		}
	} catch (const ClientError& exc) {
		L_EXC("ERROR: %s", exc.what());
		delete_files(restored_db);
		RETURN(1);
	} catch (const Xapian::Error& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		delete_files(restored_db);
		RETURN(1);
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", exc.what());
		delete_files(restored_db);
		RETURN(1);
	}
	delete_files(restored_db);
#else
	L_ERR("XAPIAND_DATABASE_WAL is not activated");
#endif
	RETURN(1);
}
