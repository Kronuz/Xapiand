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

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint8_t, std::uint64_t
#include <functional>              // for std::function
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <variant>                 // for std::variant
#include <vector>                  // for std::vector

#include "engine/types.h"          // for engine::docid, engine::rev


// The commands a shard's replication group logs (docs/design/cluster-release.md D3, D12, D17; contract
// xapiand:command and xapiand:chunked-command). The leader prepares each write into a command, and every voter
// applies the command in log order. A command whose entry would exceed the group's `command_bytes` is proposed
// as chunks into a stream and a final entry that names ranges of it.

// The largest log entry, header included: consensus's `command_bytes`, the frame cap (D12).
inline constexpr std::size_t COMMAND_ENTRY_BYTES = 1024 * 1024;

// What a prepared write does.
enum class CommandOp : std::uint8_t {
	add = 1,
	replace = 2,
	replace_key = 3,
	erase = 4,
	erase_key = 5,
	set_metadata = 6,
};

// Where a chunked command's bytes go: its staging file, or the volume of its own upload (D12).
enum class CommandStream : std::uint8_t {
	staging = 0,
	upload = 1,
};

// Bytes a command carries in place, or a range of what its chunks wrote into a stream.
struct CommandBytes {
	std::string bytes;                          // in place, when not staged
	bool staged = false;
	CommandStream stream = CommandStream::staging;
	std::uint64_t offset = 0;
	std::uint64_t length = 0;

	std::uint64_t size() const noexcept { return staged ? length : bytes.size(); }
	bool operator==(const CommandBytes&) const = default;
};

// A blob of a write's data, stored apart from its document: the content type and the record as prepare
// compressed it, which apply writes verbatim (so its locator is a function of the log).
struct CommandBlob {
	std::string ct_type;
	CommandBytes record;

	bool operator==(const CommandBlob&) const = default;
};

// One prepared write.
struct CommandWrite {
	CommandOp op = CommandOp::replace_key;
	engine::docid did = 0;                 // the id it addresses or, prepared, lands at
	std::string key;                       // replace_key, erase_key: the key; set_metadata: the metadata key
	std::optional<engine::rev> expected;   // the version apply requires; 0 means absent
	CommandBytes document;                 // engine::Document::serialize() bytes; set_metadata: the value
	std::vector<CommandBlob> blobs;

	bool operator==(const CommandWrite&) const = default;
};

// A command: one write, or a batch of them (a RESTORE's, D17), prepared at one schema version.
struct Command {
	std::uint64_t schema_version = 0;
	std::vector<CommandWrite> writes;

	bool operator==(const Command&) const = default;
};

// A chunked command's id: the proposing leader's term in the high 32 bits, a sequence below.
constexpr std::uint64_t chunked_id(std::uint64_t term, std::uint64_t sequence) noexcept {
	return term << 32 | (sequence & 0xffffffffu);
}

// The term every entry of a chunked command lands in, or it fails: a higher term's first entry retires it (D12).
constexpr std::uint64_t chunked_term(std::uint64_t id) noexcept {
	return id >> 32;
}

// A chunk of a chunked command: bytes at an offset of one of its streams.
struct CommandChunk {
	std::uint64_t id = 0;
	CommandStream stream = CommandStream::staging;
	std::uint64_t offset = 0;
	std::string_view bytes;
};

// A chunked command's final entry: the command, whose staged bytes are ranges of what the chunks of `id` wrote.
struct CommandFinal {
	std::uint64_t id = 0;
	Command command;
};

// A log entry as the application reads it.
using CommandEntry = std::variant<Command, CommandChunk, CommandFinal>;

// One entry's bytes. A whole command or a final entry raises errc::invalid when it exceeds `limit`.
std::string encode_entry(const Command& command, std::size_t limit = COMMAND_ENTRY_BYTES);
std::string encode_entry(const CommandChunk& chunk);
std::string encode_entry(const CommandFinal& final, std::size_t limit = COMMAND_ENTRY_BYTES);

// The bytes `write` takes in a command's entry: an entry is its header and the sum of its writes' (D17's packing).
std::size_t encoded_size(const CommandWrite& write);

// Reads an entry; raises errc::corrupt on bytes that are not one. A chunk's bytes view `entry`.
CommandEntry decode_entry(std::string_view entry);

// The most bytes a chunk entry of `id` at `offset` carries within `limit`.
std::size_t chunk_capacity(std::uint64_t id, std::uint64_t offset, std::size_t limit = COMMAND_ENTRY_BYTES);

// The entries that propose `command`: the command alone when it fits `limit`, otherwise chunks of `id` into its
// staging stream and a final entry. Its largest in-place bytes are staged first, until the final entry fits;
// when nothing is left to stage and it still does not, raises errc::invalid.
std::vector<std::string> split_command(const Command& command, std::uint64_t id, std::size_t limit = COMMAND_ENTRY_BYTES);

// The command with its staged bytes read back through `read(stream, offset, length)`.
Command resolve_command(Command command, const std::function<std::string(CommandStream, std::uint64_t, std::uint64_t)>& read);
