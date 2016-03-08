/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "test_storage.h"

#include "../src/log.h"
#include "../src/storage.h"
#include "../src/utils.h"


#pragma pack(push, 1)
struct StorageBinFooterChecksum {
	uint32_t checksum;
	// uint8_t magic;

	inline void init(void* /*param*/, uint32_t  checksum_) {
		// magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	inline void validate(void* /*param*/, uint32_t checksum_) {
		// if (magic != STORAGE_BIN_FOOTER_MAGIC) {
		// 	throw MSG_StorageCorruptVolume("Bad bin footer magic number");
		// }
		if (checksum != checksum_) {
			throw MSG_StorageCorruptVolume("Bad bin checksum");
		}
	}
};
#pragma pack(pop)


const std::string volumen_name("examples/volumen0");


int test_storage_data(int flags) {
	Storage<StorageHeader, StorageBinHeader, StorageBinFooterChecksum> _storage;
	_storage.open(volumen_name, STORAGE_CREATE_OR_OPEN | flags);

	std::string data;
	int cont_write = 0;
	for (int i = 0; i < 10240; ++i) {
		_storage.write(data);
		data.append(1, random_int('0', 'z'));
		++cont_write;
	}

	int cont_read = 0;
	try {
		while (true) {
			_storage.read().size();
			++cont_read;
		}
	} catch (const StorageException& er) {
		L_ERR(nullptr, "Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const LZ4Exception& er) {
		L_ERR(nullptr, "Read: [%d] %s\n", cont_read, er.get_context());
	} catch (const std::exception& er) {
		L_ERR(nullptr, "Read: [%d] %s\n", cont_read, er.what());
	}

	unlink(volumen_name.c_str());

	return cont_read != cont_write;
}
