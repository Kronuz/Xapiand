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

#include "database/command.h"

#include <algorithm>               // for std::stable_sort, std::min
#include <numeric>                 // for std::iota
#include <utility>                 // for std::move

#include "engine/error.h"          // for engine::Error, engine::errc
#include "varint.hh"               // for varint::leb128


/*
 * An entry is its kind and the format's version, then the kind's fields; integers are LEB128 and strings are
 * their length and bytes:
 *
 *   command  1 1 schema_version count write...
 *   chunk    2 1 id stream offset bytes...        (the bytes run to the entry's end)
 *   final    3 1 id schema_version count write...
 *
 *   write    op flags did key [expected] document blobs blob...   (flags: 1 has_expected)
 *   blob     ct_type record
 *   bytes    0 string | 1 stream offset length
 */

namespace {

constexpr std::uint8_t KIND_COMMAND = 1;
constexpr std::uint8_t KIND_CHUNK = 2;
constexpr std::uint8_t KIND_FINAL = 3;
constexpr std::uint8_t VERSION = 1;
constexpr std::uint8_t HAS_EXPECTED = 1;

// Bytes past the payload a chunk entry takes at most: kind, version, id, stream, offset.
constexpr std::size_t CHUNK_HEADER_MAX = 2 + varint::leb128::max_bytes + 1 + varint::leb128::max_bytes;

void
put_byte(std::string& out, std::uint8_t byte)
{
	out.push_back(static_cast<char>(byte));
}

void
put_number(std::string& out, std::uint64_t value)
{
	char buf[varint::leb128::max_bytes];
	out.append(buf, varint::leb128::encode(buf, value));
}

void
put_string(std::string& out, std::string_view bytes)
{
	put_number(out, bytes.size());
	out.append(bytes);
}

void
put_bytes(std::string& out, const CommandBytes& bytes)
{
	if (bytes.staged) {
		put_byte(out, 1);
		put_byte(out, static_cast<std::uint8_t>(bytes.stream));
		put_number(out, bytes.offset);
		put_number(out, bytes.length);
	} else {
		put_byte(out, 0);
		put_string(out, bytes.bytes);
	}
}

void
put_command(std::string& out, const Command& command)
{
	put_number(out, command.schema_version);
	put_number(out, command.writes.size());
	for (const auto& write : command.writes) {
		put_byte(out, static_cast<std::uint8_t>(write.op));
		put_byte(out, write.expected ? HAS_EXPECTED : 0);
		put_number(out, write.did);
		put_string(out, write.key);
		if (write.expected) {
			put_number(out, *write.expected);
		}
		put_bytes(out, write.document);
		put_number(out, write.blobs.size());
		for (const auto& blob : write.blobs) {
			put_string(out, blob.ct_type);
			put_bytes(out, blob.record);
		}
	}
}

[[noreturn]] void
corrupt(const char* what)
{
	throw engine::Error(engine::errc::corrupt, std::string("Bad group command: ") + what);
}

// A reader over an entry that never reads past its end.
class Reader {
public:
	explicit Reader(std::string_view bytes) : p_(bytes.data()), end_(bytes.data() + bytes.size()) { }

	bool done() const noexcept { return p_ == end_; }

	std::uint8_t byte() {
		if (p_ == end_) {
			corrupt("truncated");
		}
		return static_cast<std::uint8_t>(*p_++);
	}

	template <typename T>
	T number() {
		T value;
		if (!varint::leb128::decode(&p_, end_, value)) {
			corrupt("a bad number");
		}
		return value;
	}

	std::string_view string() {
		const auto length = number<std::uint64_t>();
		if (length > static_cast<std::uint64_t>(end_ - p_)) {
			corrupt("a string past the end");
		}
		std::string_view out(p_, static_cast<std::size_t>(length));
		p_ += length;
		return out;
	}

	std::string_view rest() noexcept {
		std::string_view out(p_, static_cast<std::size_t>(end_ - p_));
		p_ = end_;
		return out;
	}

	CommandStream stream() {
		const auto stream = byte();
		if (stream > static_cast<std::uint8_t>(CommandStream::upload)) {
			corrupt("an unknown stream");
		}
		return static_cast<CommandStream>(stream);
	}

	CommandBytes bytes() {
		CommandBytes out;
		switch (byte()) {
			case 0:
				out.bytes = string();
				break;
			case 1:
				out.staged = true;
				out.stream = stream();
				out.offset = number<std::uint64_t>();
				out.length = number<std::uint64_t>();
				if (out.offset + out.length < out.offset) {
					corrupt("a range past the stream's end");
				}
				break;
			default:
				corrupt("an unknown bytes tag");
		}
		return out;
	}

	Command command() {
		Command command;
		command.schema_version = number<std::uint64_t>();
		const auto count = number<std::uint64_t>();
		if (count > static_cast<std::uint64_t>(end_ - p_)) {
			corrupt("more writes than bytes");   // each write takes bytes: a count past them is a bad one
		}
		command.writes.reserve(static_cast<std::size_t>(count));
		for (std::uint64_t i = 0; i < count; ++i) {
			auto& write = command.writes.emplace_back();
			const auto op = byte();
			if (op < static_cast<std::uint8_t>(CommandOp::add) || op > static_cast<std::uint8_t>(CommandOp::set_metadata)) {
				corrupt("an unknown operation");
			}
			write.op = static_cast<CommandOp>(op);
			const auto flags = byte();
			if ((flags & ~HAS_EXPECTED) != 0) {
				corrupt("unknown flags");
			}
			write.did = number<engine::docid>();
			write.key = string();
			if (flags & HAS_EXPECTED) {
				write.expected = number<engine::rev>();
			}
			write.document = bytes();
			const auto blobs = number<std::uint64_t>();
			if (blobs > static_cast<std::uint64_t>(end_ - p_)) {
				corrupt("more blobs than bytes");
			}
			write.blobs.reserve(static_cast<std::size_t>(blobs));
			for (std::uint64_t b = 0; b < blobs; ++b) {
				auto& blob = write.blobs.emplace_back();
				blob.ct_type = string();
				blob.record = bytes();
			}
		}
		return command;
	}

private:
	const char* p_;
	const char* end_;
};

void
check_limit(const std::string& entry, std::size_t limit)
{
	if (entry.size() > limit) {
		throw engine::Error(engine::errc::invalid, "A group command of " + std::to_string(entry.size()) +
			" bytes exceeds the entry limit of " + std::to_string(limit) + " bytes");
	}
}

}  // namespace


std::string
encode_entry(const Command& command, std::size_t limit)
{
	std::string out;
	put_byte(out, KIND_COMMAND);
	put_byte(out, VERSION);
	put_command(out, command);
	check_limit(out, limit);
	return out;
}


std::string
encode_entry(const CommandChunk& chunk)
{
	std::string out;
	out.reserve(CHUNK_HEADER_MAX + chunk.bytes.size());
	put_byte(out, KIND_CHUNK);
	put_byte(out, VERSION);
	put_number(out, chunk.id);
	put_byte(out, static_cast<std::uint8_t>(chunk.stream));
	put_number(out, chunk.offset);
	out.append(chunk.bytes);
	return out;
}


std::string
encode_entry(const CommandFinal& final, std::size_t limit)
{
	std::string out;
	put_byte(out, KIND_FINAL);
	put_byte(out, VERSION);
	put_number(out, final.id);
	put_command(out, final.command);
	check_limit(out, limit);
	return out;
}


CommandEntry
decode_entry(std::string_view entry)
{
	Reader in(entry);
	const auto kind = in.byte();
	if (in.byte() != VERSION) {
		corrupt("an unknown version");
	}
	switch (kind) {
		case KIND_COMMAND: {
			auto command = in.command();
			if (!in.done()) {
				corrupt("bytes past the command");
			}
			return command;
		}
		case KIND_CHUNK: {
			CommandChunk chunk;
			chunk.id = in.number<std::uint64_t>();
			chunk.stream = in.stream();
			chunk.offset = in.number<std::uint64_t>();
			chunk.bytes = in.rest();
			return chunk;
		}
		case KIND_FINAL: {
			CommandFinal final;
			final.id = in.number<std::uint64_t>();
			final.command = in.command();
			if (!in.done()) {
				corrupt("bytes past the command");
			}
			return final;
		}
		default:
			corrupt("an unknown kind");
	}
}


std::size_t
chunk_capacity(std::uint64_t id, std::uint64_t offset, std::size_t limit)
{
	const std::size_t header = 2 + varint::leb128::size(id) + 1 + varint::leb128::size(offset);
	return limit > header ? limit - header : 0;
}


std::vector<std::string>
split_command(const Command& command, std::uint64_t id, std::size_t limit)
{
	std::string whole;
	put_byte(whole, KIND_COMMAND);
	put_byte(whole, VERSION);
	put_command(whole, command);
	if (whole.size() <= limit) {
		return {std::move(whole)};
	}

	// The in-place bytes, largest first (ties in command order), staged one at a time until the final entry fits.
	Command staged = command;
	std::vector<CommandBytes*> parts;
	for (auto& write : staged.writes) {
		parts.push_back(&write.document);
		for (auto& blob : write.blobs) {
			parts.push_back(&blob.record);
		}
	}
	std::vector<std::size_t> order(parts.size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return parts[a]->bytes.size() > parts[b]->bytes.size(); });

	std::string stream;
	std::string final;
	auto encode_final = [&] {
		final.clear();
		put_byte(final, KIND_FINAL);
		put_byte(final, VERSION);
		put_number(final, id);
		put_command(final, staged);
	};
	encode_final();
	for (auto at = order.begin(); final.size() > limit; ++at) {
		if (at == order.end() || (*parts[*at]).staged || (*parts[*at]).bytes.empty()) {
			throw engine::Error(engine::errc::invalid, "A group command of " + std::to_string(final.size()) +
				" bytes does not fit the entry limit of " + std::to_string(limit) + " bytes with its bytes staged");
		}
		auto& part = *parts[*at];
		part.staged = true;
		part.stream = CommandStream::staging;
		part.offset = stream.size();
		part.length = part.bytes.size();
		stream.append(part.bytes);
		part.bytes.clear();
		part.bytes.shrink_to_fit();
		encode_final();
	}

	std::vector<std::string> entries;
	std::string_view rest(stream);
	for (std::uint64_t offset = 0; !rest.empty();) {
		const auto take = std::min(rest.size(), chunk_capacity(id, offset, limit));
		if (take == 0) {
			throw engine::Error(engine::errc::invalid, "The entry limit leaves no room for a chunk's bytes");
		}
		entries.push_back(encode_entry(CommandChunk{id, CommandStream::staging, offset, rest.substr(0, take)}));
		rest.remove_prefix(take);
		offset += take;
	}
	entries.push_back(std::move(final));
	return entries;
}


Command
resolve_command(Command command, const std::function<std::string(CommandStream, std::uint64_t, std::uint64_t)>& read)
{
	auto resolve = [&](CommandBytes& part) {
		if (part.staged) {
			auto bytes = read(part.stream, part.offset, part.length);
			if (bytes.size() != part.length) {
				corrupt("a staged range shorter than its length");
			}
			part = CommandBytes{};
			part.bytes = std::move(bytes);
		}
	};
	for (auto& write : command.writes) {
		resolve(write.document);
		for (auto& blob : write.blobs) {
			resolve(blob.record);
		}
	}
	return command;
}
