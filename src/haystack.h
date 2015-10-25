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

#pragma once

#include "xapiand.h"

#include "sparsehash/sparse_hash_map"
#include <string>


class Haystack
{
	struct NeedleIndex {
		uint64_t key;  // 64-bit object key
		uint32_t offset;  // Offset of the needle in the haystack volume / 8
		uint32_t size;  // Size of the needle / 8
	};

	struct NeedleHeader {
		uint64_t magic; // Magic number used to find the next possible needle during recovery
		uint64_t cookie;  // Security cookie supplied by client to prevent brute force attacks
		uint64_t key;  // 64-bit object key
		uint8_t flags;  // File flags
		size_t size;  // Data size
		// data goes here...
	};

	struct NeedleFooter {
		uint64_t magic;  // Magic number used to find possible needle end during recovery
		uint64_t checksum;  // Checksum of the data portion of the needle
		// padding to align total needle size to 8 bytes
	};

	unsigned int volumes;
	std::string path;
	google::sparse_hash_map<uint64_t, std::pair<uint32_t, uint32_t>> index;

	bool read_volume(unsigned int volume);
public:
	Haystack(const std::string &path_);
	~Haystack();
};
