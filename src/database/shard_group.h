/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <array>                   // for std::array
#include <atomic>                  // for std::atomic
#include <cstdint>                 // for std::uint64_t
#include <map>                     // for std::map
#include <memory>                  // for std::unique_ptr
#include <mutex>                   // for std::mutex
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <thread>                  // for std::thread
#include <vector>                  // for std::vector

#include "database/command.h"      // for Command
#include "engine/document.h"       // for engine::Document
#include "engine/shard.h"          // for engine::PreparedWrite
#include "prospect/index.hh"       // for prospect::index, prospect::transaction
#include "completion/seam.h"      // for kronuz::journal::InlineCompletion
#include "journal/host.h"          // for kronuz::journal::Host
#include "strata/group/group.hh"   // for strata::group::group, strata::group::loop
#include "task.hh"                 // for task::blocking_scheduler, task::cancel_source


// A shard's replication group applies its commands through this application (docs/design/cluster-release.md
// D3, D12, D16; docs/mechanisms/journal-durability.md). Its files are the group's file set beside the index:
// blob volumes `vol-N` and chunked commands' staging `stage-ID`. Every function reads the entry's starting state
// only, so a replica, a replay and a retry agree: blobs are written at the offsets the state names, never
// appended, and volume numbers are allocated in log order.

// The bytes past which apply rotates a volume: master's data-storage volume (storage.h, 2^32 8-byte units).
inline constexpr std::uint64_t VOLUME_BYTES = 32ull << 30;

// What the group keeps beside strata, in its checkpoint bundle (contract xapiand:volume-file-set).
struct ShardGroupState {
	// A chunked command whose final entry has not applied, by id (its id names its term): the volume its upload
	// stream writes (0 until its first upload chunk).
	struct Open {
		std::uint64_t upload = 0;
		bool operator==(const Open&) const = default;
	};

	std::uint64_t volume = 1;      // the volume blobs are written into
	std::uint64_t length = 0;      // its length
	std::uint64_t next = 2;        // the next volume number apply allocates
	std::map<std::uint64_t, Open> open;

	bool operator==(const ShardGroupState&) const = default;
};

// Where a command's blobs land: per write, per blob, (volume, offset); and the state after them.
struct BlobPlacement {
	std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>> at;
	std::vector<std::uint64_t> sealed;   // volumes rotated out, in order
	ShardGroupState after;
};

// Places a command's blob records from `state`: in the current volume, rotating it when a record would take it
// past VOLUME_BYTES; a record staged in an upload stream stays in its upload's volume.
BlobPlacement place_blobs(const ShardGroupState& state, const Command& command, std::uint64_t id = 0);

// A prepared write as its command's (D3): the target, the leader's expectation and the document's bytes as prepare
// left them (null `doc` for an erase). ShardApplication::transaction reads it back on every voter.
CommandWrite command_write(const engine::PreparedWrite& prepared, engine::Document* doc);

std::string volume_name(std::uint64_t volume);
std::string staging_name(std::uint64_t id);

class ShardApplication {
public:
	using layout = prospect::search_layout;
	using state = ShardGroupState;

	// `index` is the catalog slot's (D169): the transactions are built against its field table at apply.
	explicit ShardApplication(const prospect::index& index) : index_(&index) { }

	void effects(const state& s, const strata::group::entry& e, strata::group::file_set& files);
	std::optional<prospect::transaction> transaction(const state& s, const strata::group::entry& e) const;
	void advance(state& s, const strata::group::entry& e);
	std::string encode(const state& s) const;
	state decode(std::string_view bytes) const;

private:
	// The command an entry applies, its staged bytes read by its effects (they read the file set; the
	// transaction, which follows them at the same entry, cannot).
	struct Resolved {
		std::uint64_t index = 0;
		std::optional<Command> command;   // none: not a command, or a final whose chunks were retired
		std::uint64_t id = 0;             // a final's
	};

	const Resolved& resolved(const state& s, const strata::group::entry& e) const;

	const prospect::index* index_;
	mutable Resolved resolved_;
};


// Where the process's journal lives, relative to the data directory.
inline constexpr const char* JOURNAL_DIRECTORY = ".journal";

// The process's journal (docs/design/cluster-release.md D2): one journal host over a directory of the data root, the
// group loop that runs its Driver on a thread of its own, and the chore threads the groups apply on. A shard's group
// borrows its catalog slot's directory and index (D169), so the catalog opens and closes it (ShardEndpoint). Every
// group is closed before this is destroyed.
class ShardGroups {
public:
	using group_type = strata::group::group<ShardApplication>;

	// The journal host over `directory`, made when missing; `chores` threads apply the groups' entries.
	explicit ShardGroups(const std::string& directory, std::size_t chores = 2);
	~ShardGroups();

	ShardGroups(const ShardGroups&) = delete;
	ShardGroups& operator=(const ShardGroups&) = delete;

	strata::group::loop& loop() noexcept { return *loop_; }
	task::scheduler_ref chore(kronuz::consensus::GroupId id) noexcept;

	// A lone voter's group of the index with `uuid` (its id and journal identity), with the record its last close
	// in this process kept, so the reopen resumes its term with no election.
	group_type::config config(const std::array<char, 16>& uuid);
	void closed(kronuz::consensus::GroupId id, std::optional<std::string> record);

	static kronuz::consensus::GroupId group_id(const std::array<char, 16>& uuid) noexcept;

private:
	// A task scheduler driven by a thread of its own until end().
	struct Driven {
		task::blocking_scheduler scheduler;
		std::atomic<bool> quit{false};
		std::thread thread;
		void begin();
		void wake();
		void end();
		task::scheduler_ref ref() { return task::scheduler_ref::of(scheduler); }
	};

	kronuz::journal::InlineCompletion io_;
	std::unique_ptr<kronuz::journal::Host> host_;
	Driven loop_thread_;
	std::vector<std::unique_ptr<Driven>> chores_;
	std::unique_ptr<strata::group::loop> loop_;
	task::cancel_source stop_;
	std::atomic<bool> turns_done_{false};
	std::atomic<bool> driver_done_{false};
	std::mutex closed_mtx_;
	std::map<kronuz::consensus::GroupId, std::string> closed_;   // under closed_mtx_
};


// A shard's group and its application, over the catalog slot's directory and index.
struct ShardGroup {
	ShardApplication app;
	ShardGroups::group_type group;
	std::optional<ChunkedIds> ids;   // this leader's chunked-command ids, on the loop

	ShardGroup(ShardGroups& groups, kronuz::journal::IO& directory, prospect::index& index);

	// Blocks the calling thread (never the loop or the chore) until the group applied everything committed when
	// it was called: a linearizable read's barrier (D4).
	void catch_up();
};
