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

#include <cstdint>                 // for std::uint64_t
#include <map>                     // for std::map
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector

#include "database/command.h"      // for Command
#include "prospect/index.hh"       // for prospect::index, prospect::transaction
#include "strata/group/group.hh"   // for strata::group::entry, strata::group::file_set


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
