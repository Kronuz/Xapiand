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

#include "test_storage.h"

#include "../src/storage.h"
#include "../src/random.hh"
#include "utils.h"


const std::string path_test_storage = std::string(FIXTURES_PATH) + "/examples/";


#pragma pack(push, 1)
struct StorageBinBadHeader1 {
	// uint8_t magic;
	uint32_t aux;
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void* /*param*/, void* /*args*/, uint32_t size_, uint8_t flags_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void* /*param*/, void* /*args*/) {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	THROW(StorageCorruptVolume, "Bad bin header magic number");
		// }
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Bin deleted");
		}
	}
};


struct StorageBinBadHeader2 {
	// uint8_t magic;
	uint8_t flags;  // required
	uint64_t size;  // required

	inline void init(void* /*param*/, void* /*args*/, uint32_t size_, uint8_t flags_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void* /*param*/, void* /*args*/) {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	THROW(StorageCorruptVolume, "Bad bin header magic number");
		// }
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Bin deleted");
		}
	}
};


struct StorageBinBadHeader3 {
	// uint8_t magic;
	char aux[16];
	uint8_t flags;  // required
	uint32_t size;  // required

	inline void init(void* /*param*/, void* /*args*/, uint32_t size_, uint8_t flags_) {
		// magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	inline void validate(void* /*param*/, void* /*args*/) {
		// if (magic != STORAGE_BIN_HEADER_MAGIC) {
		// 	THROW(StorageCorruptVolume, "Bad bin header magic number");
		// }
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Bin deleted");
		}
	}
};


struct StorageBinFooterChecksum {
	uint32_t checksum;
	// uint8_t magic;

	inline void init(void* /*param*/, void* /*args*/, uint32_t  checksum_) {
		// magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(void* /*param*/, void* /*args*/, uint32_t checksum_) {
		// if (magic != STORAGE_BIN_FOOTER_MAGIC) {
		// 	THROW(StorageCorruptVolume, "Bad bin footer magic number");
		// }
		if (checksum != checksum_) {
			THROW(StorageCorruptVolume, "Bad bin checksum");
		}
	}
};
#pragma pack(pop)


const std::string volume_name(path_test_storage + "volume0");


static const std::vector<std::string> small_files({
	path_test_storage + "compressor/Small_File1.txt",
	path_test_storage + "compressor/Small_File2.txt",
	path_test_storage + "compressor/Small_File3.txt",
	path_test_storage + "compressor/Small_File4.txt"
});


static const std::vector<std::string> big_files({
	path_test_storage + "compressor/Big_File1.jpg",
	path_test_storage + "compressor/Big_File2.pdf",
	path_test_storage + "compressor/Big_File3.pdf",
	path_test_storage + "compressor/Big_File4.pdf",
	path_test_storage + "compressor/Big_File5.pdf"
});


int test_storage_data(int flags) {
	INIT_LOG
	Storage<StorageHeader, StorageBinHeader, StorageBinFooterChecksum> _storage("", nullptr);
	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);

	std::string data;
	int cont_write = 0;
	for (int i = 0; i < 5120; ++i) {
		_storage.write(data);
		data.append(1, random_int('\x00', '\xff'));
		++cont_write;
	}
	_storage.close();

	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
	for (int i = 5120; i < 10240; ++i) {
		_storage.write(data);
		data.append(1, random_int('\x00', '\xff'));
		++cont_write;
	}

	int cont_read = 0;
	try {
		while (true) {
			size_t r;
			char buf[LZ4_BLOCK_SIZE];
			while ((r = _storage.read(buf, sizeof(buf))));
			++cont_read;
		}
	} catch (const StorageException& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const LZ4Exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const std::exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.what());
	}

	unlink(volume_name.c_str());

	RETURN(cont_read != cont_write);
}


int test_storage_file(int flags) {
	INIT_LOG
	Storage<StorageHeader, StorageBinHeader, StorageBinFooterChecksum> _storage("", nullptr);
	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);

	int cont_write = 0;
	for (const auto& filename : small_files) {
		_storage.write_file(filename);
		++cont_write;
	}

	for (const auto& filename : big_files) {
		_storage.write_file(filename);
		++cont_write;
	}
	_storage.close();

	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
	for (const auto& filename : small_files) {
		_storage.write_file(filename);
		++cont_write;
	}

	for (const auto& filename : big_files) {
		_storage.write_file(filename);
		++cont_write;
	}

	int cont_read = 0;
	try {
		while (true) {
			size_t r;
			char buf[LZ4_BLOCK_SIZE];
			while ((r = _storage.read(buf, sizeof(buf))));
			++cont_read;
		}
	} catch (const StorageException& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const LZ4Exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const std::exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.what());
	}

	unlink(volume_name.c_str());

	RETURN(cont_read != cont_write);
}


int test_storage_bad_headers() {
	INIT_LOG
	int res = 0;

	try {
		Storage<StorageHeader, StorageBinBadHeader1, StorageBinFooterChecksum> _storage("", nullptr);
		res = 1;
	} catch (const std::exception& e) {
		L_ERR("Bad header (1): %s", e.what());
	}

	try {
		Storage<StorageHeader, StorageBinBadHeader2, StorageBinFooterChecksum> _storage("", nullptr);
		res = 1;
	} catch (const std::exception& e) {
		L_ERR("Bad header (2): %s", e.what());
	}

	try {
		Storage<StorageHeader, StorageBinBadHeader3, StorageBinFooterChecksum> _storage("", nullptr);
		res = 1;
	} catch (const std::exception& e) {
		L_ERR("Bad header (3): %s", e.what());
	}

	RETURN(res);
}


int test_storage_exception_write(int flags) {
	INIT_LOG
	std::atomic_bool finish(false);
	std::mutex mtx;
	Storage<StorageHeader, StorageBinHeader, StorageBinFooterChecksum> _storage("", nullptr);

	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);

	auto write_storage = std::thread([&]() {
		std::string data;
		for (int i = 0; i < 5120; ++i) {
			std::unique_lock<std::mutex> lk(mtx);
			try {
				_storage.write(data);
				lk.unlock();
			} catch (const StorageException& er) {
				_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
				lk.unlock();
			}
			lk.lock();
			data.append(1, random_int(0, 255));
			lk.unlock();
		}
		finish.store(true);
	});

	auto interrupt_storage = std::thread([&]() {
		while (true) {
			std::unique_lock<std::mutex> lk(mtx);
			uint64_t ran = random_int(10, 20);
			lk.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(ran));
			if (!finish.load()) {
				lk.lock();
				_storage.close();
				lk.unlock();
			} else {
				return;
			}
		}
	});

	write_storage.detach();
	interrupt_storage.join();

	_storage.close();
	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
	int cont_read = 0;
	try {
		while (true) {
			size_t r;
			char buf[LZ4_BLOCK_SIZE];
			while ((r = _storage.read(buf, sizeof(buf))));
			++cont_read;
		}
	} catch (const StorageEOF& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
		unlink(volume_name.c_str());
		RETURN(0);
	} catch (const std::exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.what());
		unlink(volume_name.c_str());
		RETURN(1);
	}
}


int test_storage_exception_write_file(int flags) {
	INIT_LOG
	std::atomic_bool finish(false);
	std::mutex mtx;
	Storage<StorageHeader, StorageBinHeader, StorageBinFooterChecksum> _storage("", nullptr);

	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);

	auto write_storage = std::thread([&]() {
		for (const auto& filename : small_files) {
			std::unique_lock<std::mutex> lk(mtx);
			try {
				_storage.write_file(filename);
				lk.unlock();
			} catch (const StorageException& er) {
				_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
				lk.unlock();
			}
		}

		for (const auto& filename : big_files) {
			std::unique_lock<std::mutex> lk(mtx);
			try {
				_storage.write_file(filename);
				lk.unlock();
			} catch (const StorageException& er) {
				_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
				lk.unlock();
			}
		}

		for (const auto& filename : small_files) {
			std::unique_lock<std::mutex> lk(mtx);
			try {
				_storage.write_file(filename);
				lk.unlock();
			} catch (const StorageException& er) {
				_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
				lk.unlock();
			}
		}

		finish.store(true);
	});

	auto interrupt_storage = std::thread([&]() {
		while (true) {
			std::unique_lock<std::mutex> lk(mtx);
			uint64_t ran = random_int(10, 20);
			lk.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(ran));
			if (!finish.load()) {
				lk.lock();
				_storage.close();
				lk.unlock();
			} else {
				return;
			}
		}
	});

	write_storage.detach();
	interrupt_storage.join();

	_storage.close();
	_storage.open(volume_name, STORAGE_CREATE_OR_OPEN | flags);
	int cont_read = 0;
	try {
		while (true) {
			size_t r;
			char buf[LZ4_BLOCK_SIZE];
			while ((r = _storage.read(buf, sizeof(buf))));
			++cont_read;
		}
	} catch (const StorageEOF& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.get_context());
		unlink(volume_name.c_str());
		RETURN(0);
	} catch (const std::exception& er) {
		L_ERR("Read: [%d] %s\n", cont_read, er.what());
		unlink(volume_name.c_str());
		RETURN(1);
	}
}
