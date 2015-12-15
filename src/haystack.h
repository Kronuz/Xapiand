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

#include <string>
#include <vector>
#include <memory>


#define EPOS (-2)
#define EBADSTATE (-3)
#define ENOID (-4)
#define EOFH (-5)
#define EOFB (-6)
#define EOFF (-7)
#define ECORRUPTH (-8)
#define ECORRUPTF (-9)
#define EBADID (-10)
#define EBADCOOKIE (-11)
#define EBADCHECKSUM (-12)


using chunk_size_t = uint32_t;
using did_t        = uint32_t;  // Document ID
using offset_t     = uint32_t;
using cookie_t     = uint16_t;
using checksum_t   = uint32_t;
using magic_t      = uint32_t;


struct VolumeError : std::exception { };


class HaystackFile;
class HaystackIndexedFile;


class HaystackVolume {
	friend HaystackFile;

	offset_t eof_offset;

	std::string data_path;
	int data_file;

public:
	HaystackVolume(const std::string& path, bool writable);
	~HaystackVolume();

	offset_t offset();
};


class HaystackFile {
	#pragma pack(push, 1)
	struct NeedleHeader {
		struct Head {
			magic_t magic; // Magic number used to find the next possible needle during recovery
			did_t id;  // document id (if 0, it's deleted)
			cookie_t cookie;  // Security cookie supplied by client to prevent brute force attacks
			size_t size;  // Full size (uncompressed)
			// data goes here...
		} head;
		chunk_size_t chunk_size;
	} header;

	struct NeedleFooter {
		// chunk_size_t zero
		struct Foot {
			magic_t magic;  // Magic number used to find possible needle end during recovery
			checksum_t checksum;  // Checksum of the data portion of the needle
			// padding to align total needle size to 8 bytes
		} foot;
	} footer;

	#pragma pack(pop)
	char* buffer;
	ssize_t buffer_size;
	ssize_t available_buffer;
	chunk_size_t next_chunk_size;

	did_t wanted_id;
	cookie_t wanted_cookie;

	std::shared_ptr<HaystackVolume> volume;

protected:
	offset_t current_offset;

private:
	off_t real_offset;

	enum class State {
		OPENED,
		WRITING,
		READING,
		ERROR,
		H_EOF,
	};

	State state;

	void write_header(size_t size);
	size_t write_chunk(const char* data, size_t size);
	offset_t write_footer();
	offset_t _seek(offset_t offset_);

public:
	HaystackFile(const std::shared_ptr<HaystackVolume> &volume_, did_t id_, cookie_t cookie_);
	~HaystackFile();

	did_t id();
	size_t size();
	offset_t offset();
	cookie_t cookie();
	checksum_t checksum();

	offset_t seek(offset_t offset_);
	offset_t next();
	offset_t rewind();

	ssize_t write(const char* data, size_t size);
	ssize_t read(char* data, size_t size);

	offset_t commit();
};


class HaystackIndex {
	std::string index_path;
	int index_file;

	offset_t index_base;
	offset_t index_touched;
	std::vector<offset_t> index;

public:
	HaystackIndex(const std::string& path, bool writable);
	~HaystackIndex();

	offset_t get_offset(did_t id);
	void set_offset(did_t id, offset_t offset);
	void flush();
};


class Haystack {
	friend HaystackIndexedFile;

	std::shared_ptr<HaystackIndex> index;
	std::shared_ptr<HaystackVolume> volume;

public:
	Haystack(const std::string& path, bool writable=false);

	HaystackIndexedFile open(did_t id, cookie_t cookie, int mode=0);
	void flush();
};


class HaystackIndexedFile : public HaystackFile {
	std::shared_ptr<HaystackIndex> index;

public:
	HaystackIndexedFile(Haystack* haystack, did_t id_, cookie_t cookie_);

	offset_t commit();
};
