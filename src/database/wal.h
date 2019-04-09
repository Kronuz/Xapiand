/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#pragma once

#include "config.h"                         // for XAPIAND_REMOTE_SERVERPORT

#if XAPIAND_DATABASE_WAL

#include <array>                            // for std::array
#include <cassert>                          // for assert
#include <chrono>                           // for std::chrono
#include <memory>                           // for std::unique_ptr
#include <string>                           // for std::string
#include <sys/types.h>                      // for uint32_t, uint8_t, ssize_t
#include <unordered_map>                    // for std::unordered_map
#include <utility>                          // for pair, make_pair

#include "blocking_concurrent_queue.h"      // for BlockingConcurrentQueue
#include "cuuid/uuid.h"                     // for UUID
#include "endpoint.h"                       // for Endpoint
#include "lru.h"                            // for lru::LRU
#include "storage.h"                        // for Storage, STORAGE_BLOCK_SIZE, StorageCorruptVolume...
#include "thread.hh"                        // for Thread, ThreadPolicyType::*
#include "xapian.h"                         // for Xapian::docid, Xapian::termcount, Xapian::Document


class MsgPack;
class Shard;
struct WalHeader;


#define WAL_SLOTS ((STORAGE_BLOCK_SIZE - sizeof(WalHeader::StorageHeaderHead)) / sizeof(uint32_t))


struct WalHeader {
	struct StorageHeaderHead {
		uint32_t offset;
		Xapian::rev revision;
		std::array<unsigned char, 16> uuid;
	} head;

	uint32_t slot[WAL_SLOTS];

	void init(void* param, void* args);
	void validate(void* param, void* args);
};


#pragma pack(push, 1)
struct WalBinHeader {
	uint8_t magic;
	uint8_t flags;  // required
	uint32_t size;  // required

	void init(void*, void*, uint32_t size_, uint8_t flags_) {
		magic = STORAGE_BIN_HEADER_MAGIC;
		size = size_;
		flags = (0 & ~STORAGE_FLAG_MASK) | flags_;
	}

	void validate(void*, void*) {
		if (magic != STORAGE_BIN_HEADER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad line header magic number");
		}
		if (flags & STORAGE_FLAG_DELETED) {
			THROW(StorageNotFound, "Line deleted");
		}
	}
};


struct WalBinFooter {
	uint32_t checksum;
	uint8_t magic;

	void init(void*, void*, uint32_t checksum_) {
		magic = STORAGE_BIN_FOOTER_MAGIC;
		checksum = checksum_;
	}

	void validate(void*, void*, uint32_t checksum_) {
		if (magic != STORAGE_BIN_FOOTER_MAGIC) {
			THROW(StorageCorruptVolume, "Bad line footer magic number");
		}
		if (checksum != checksum_) {
			THROW(StorageCorruptVolume, "Bad line checksum");
		}
	}
};
#pragma pack(pop)


class DatabaseWAL : Storage<WalHeader, WalBinHeader, WalBinFooter> {
	class iterator;

	friend WalHeader;

	bool validate_uuid;

	MsgPack to_string_document(std::string_view document, bool unserialised);
	MsgPack to_string_metadata(std::string_view document, bool unserialised);
	MsgPack to_string_line(std::string_view line, bool unserialised);
	uint32_t highest_valid_slot();

	bool open(std::string_view path, int flags, bool commit_eof = false) {
		return Storage<WalHeader, WalBinHeader, WalBinFooter>::open(path, flags, reinterpret_cast<void*>(commit_eof));
	}

public:
	static constexpr Xapian::rev max_rev = std::numeric_limits<Xapian::rev>::max() - 1;
	static constexpr uint32_t max_slot = std::numeric_limits<uint32_t>::max();

	enum class Type : uint8_t {
		COMMIT,
		REPLACE_DOCUMENT,
		DELETE_DOCUMENT,
		SET_METADATA,
		ADD_SPELLING,
		REMOVE_SPELLING,
		MAX,
	};

	UUID _uuid;
	UUID _uuid_le;
	Xapian::rev _revision;
	Shard* _shard;

	DatabaseWAL(Shard* shard);
	DatabaseWAL(std::string_view base_path_);

	iterator begin();
	iterator end();

	const UUID& get_uuid() const;
	const UUID& get_uuid_le() const;
	Xapian::rev get_revision() const;

	bool init_database();
	bool execute(bool only_committed, bool unsafe = false);
	bool execute_line(std::string_view line, bool wal_, bool send_update, bool unsafe);
	void write_line(const UUID& uuid, Xapian::rev revision, Type type, std::string_view data, bool send_update);

	MsgPack to_string(Xapian::rev start_revision, Xapian::rev end_revision, bool unserialised);

	std::pair<Xapian::rev, uint32_t> locate_revision(Xapian::rev revision);
	iterator find(Xapian::rev revision);
	std::string get_current_line(uint32_t end_off);
};


class DatabaseWAL::iterator {
	friend DatabaseWAL;

	DatabaseWAL* wal;
	std::string line;
	uint32_t end_off;

public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = std::string;
	using difference_type = std::string;
	using pointer = std::string*;
	using reference = std::string&;

	iterator(DatabaseWAL* wal_, std::string&& item_, uint32_t end_off_)
		: wal(wal_),
		  line(item_),
		  end_off(end_off_) { }

	iterator& operator++() {
		line = wal->get_current_line(end_off);
		return *this;
	}

	iterator operator=(const iterator& other) {
		wal = other.wal;
		line = other.line;
		return *this;
	}

	std::string& operator*() {
		return line;
	}

	std::string* operator->() {
		return &operator*();
	}

	std::string& value() {
		return line;
	}

	bool operator==(const iterator& other) const {
		return this == &other || line == other.line;
	}

	bool operator!=(const iterator& other) const {
		return !operator==(other);
	}

};


inline DatabaseWAL::iterator DatabaseWAL::begin() {
	return find(0);
}


inline DatabaseWAL::iterator DatabaseWAL::end() {
	return iterator(this, "", 0);
}


/*
 *  ____        _        _                  __        ___    _ __        __    _ _
 * |  _ \  __ _| |_ __ _| |__   __ _ ___  __\ \      / / \  | |\ \      / / __(_) |_ ___ _ __
 * | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \ \ /\ / / _ \ | | \ \ /\ / / '__| | __/ _ \ '__|
 * | |_| | (_| | || (_| | |_) | (_| \__ \  __/\ V  V / ___ \| |__\ V  V /| |  | | ||  __/ |
 * |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___| \_/\_/_/   \_\_____\_/\_/ |_|  |_|\__\___|_|
 *
 */

class DatabaseWALWriter;
class DatabaseWALWriterThread;


class DatabaseWALWriterTask {
	friend DatabaseWALWriter;
	friend DatabaseWALWriterThread;

	using dispatch_func = void (DatabaseWALWriterTask::*)(DatabaseWALWriterThread&);
	dispatch_func dispatcher;

	std::string path;
	UUID uuid;
	Xapian::rev revision;

	Xapian::Document doc;
	std::string key;
	std::string term_word_val;
	Xapian::termcount freq;
	Xapian::docid did;
	bool send_update;

	void write_commit(DatabaseWALWriterThread& thread);
	void write_replace_document(DatabaseWALWriterThread& thread);
	void write_delete_document(DatabaseWALWriterThread& thread);
	void write_set_metadata(DatabaseWALWriterThread& thread);
	void write_add_spelling(DatabaseWALWriterThread& thread);
	void write_remove_spelling(DatabaseWALWriterThread& thread);

public:
	DatabaseWALWriterTask() : dispatcher(nullptr) {}
	DatabaseWALWriterTask(const DatabaseWALWriterTask&) = delete;
	DatabaseWALWriterTask(DatabaseWALWriterTask&&) = default;
	DatabaseWALWriterTask& operator=(const DatabaseWALWriterTask&) = delete;
	DatabaseWALWriterTask& operator=(DatabaseWALWriterTask&&) = default;

	void operator()(DatabaseWALWriterThread& thread) {
		assert(dispatcher);
		(this->*(dispatcher))(thread);
	}

	operator bool() const {
		return dispatcher != nullptr;
	}
};


class DatabaseWALWriterThread : public Thread<DatabaseWALWriterThread, ThreadPolicyType::wal_writer> {
	friend DatabaseWALWriter;

	DatabaseWALWriter* _wal_writer;
	std::string _name;
	BlockingConcurrentQueue<DatabaseWALWriterTask> _queue;

	lru::LRU<std::string, std::unique_ptr<DatabaseWAL>> lru;

public:
	DatabaseWALWriterThread() noexcept;
	DatabaseWALWriterThread(size_t idx, DatabaseWALWriter* wal_writer) noexcept;
	DatabaseWALWriterThread& operator=(DatabaseWALWriterThread&& other);

	void operator()();
	void clear();
	DatabaseWAL& wal(const std::string& path);

	const std::string& name() const noexcept;
};


class DatabaseWALWriter {
	friend DatabaseWALWriterThread;

	std::vector<DatabaseWALWriterThread> _threads;
	const char* _format;

	std::atomic_bool _ending;
	std::atomic_bool _finished;
	std::atomic_size_t _workers;

	void execute(DatabaseWALWriterTask&& task);
	bool enqueue(DatabaseWALWriterTask&& task);

public:
	DatabaseWALWriter(const char* format, std::size_t size);

	void clear();

	bool join(std::chrono::milliseconds timeout);
	bool join(int timeout = 60000) {
		return join(std::chrono::milliseconds(timeout));
	}

	void end();
	void finish();

	std::size_t running_size();

	void write_commit(Shard& shard, bool send_update);
	void write_replace_document(Shard& shard, Xapian::docid did, Xapian::Document&& doc);
	void write_delete_document(Shard& shard, Xapian::docid did);
	void write_set_metadata(Shard& shard, const std::string& key, const std::string& val);
	void write_add_spelling(Shard& shard, const std::string& word, Xapian::termcount freqinc);
	void write_remove_spelling(Shard& shard, const std::string& word, Xapian::termcount freqdec);
};


#endif /* XAPIAND_DATABASE_WAL */
