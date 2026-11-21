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

// The group's command codec (src/database/command.h; cluster-release D3, D12, D17): entries round-trip, every
// entry of a chunked command fits the entry limit with its header, and bytes that are not an entry are refused.

#include "test_harness.h"

#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "database/command.h"
#include "engine/error.h"

namespace {

std::string pattern(std::size_t n, char seed) {
	std::string out(n, '\0');
	for (std::size_t i = 0; i < n; ++i) {
		out[i] = static_cast<char>(seed + static_cast<char>(i * 131 % 251));
	}
	return out;
}

CommandBytes inplace(std::string bytes) {
	CommandBytes out;
	out.bytes = std::move(bytes);
	return out;
}

CommandWrite write(CommandOp op, std::string document, std::vector<std::string> blobs = {}) {
	CommandWrite w;
	w.op = op;
	w.did = 7;
	w.key = "Kdoc";
	w.expected = 3;
	w.document = inplace(std::move(document));
	for (std::size_t i = 0; i < blobs.size(); ++i) {
		w.blobs.push_back({"application/octet-stream;" + std::to_string(i), inplace(std::move(blobs[i]))});
	}
	return w;
}

bool raises(engine::errc kind, const std::function<void()>& f) {
	try {
		f();
	} catch (const engine::Error& e) {
		return e.kind == kind;
	}
	return false;
}

// Applies the chunks of `entries` into streams and returns the final's command resolved from them.
Command reassemble(const std::vector<std::string>& entries, std::uint64_t id) {
	std::map<CommandStream, std::string> streams;
	Command out;
	for (const auto& entry : entries) {
		const auto decoded = decode_entry(entry);
		if (const auto* chunk = std::get_if<CommandChunk>(&decoded)) {
			CHECK_EQ(chunk->id, id);
			auto& stream = streams[chunk->stream];
			CHECK_EQ(chunk->offset, stream.size());   // in order, no gap
			stream.append(chunk->bytes);
		} else if (const auto* final = std::get_if<CommandFinal>(&decoded)) {
			CHECK_EQ(final->id, id);
			out = resolve_command(final->command, [&](CommandStream s, std::uint64_t offset, std::uint64_t length) {
				return streams[s].substr(offset, length);
			});
		} else {
			out = std::get<Command>(decoded);
		}
	}
	return out;
}

// Every op, an absent expectation, a batch, blobs and a staged range survive encode and decode.
void test_round_trip() {
	Command command;
	command.schema_version = 42;
	for (auto op : {CommandOp::add, CommandOp::replace, CommandOp::replace_key, CommandOp::erase, CommandOp::erase_key, CommandOp::set_metadata}) {
		command.writes.push_back(write(op, pattern(300, 'a'), {pattern(10, 'b'), std::string()}));
	}
	command.writes[1].expected.reset();
	command.writes[2].expected = 0;
	command.writes[3].did = 0xffffffffu;
	command.writes[4].blobs[0].record = CommandBytes{{}, true, CommandStream::upload, 1ull << 40, 5};
	const auto entry = encode_entry(command);
	const auto back = decode_entry(entry);
	REQUIRE(std::holds_alternative<Command>(back));
	CHECK(std::get<Command>(back) == command);

	const CommandFinal final{1234567, command};
	const auto f = decode_entry(encode_entry(final));
	REQUIRE(std::holds_alternative<CommandFinal>(f));
	CHECK_EQ(std::get<CommandFinal>(f).id, 1234567u);
	CHECK(std::get<CommandFinal>(f).command == command);

	const std::string bytes = pattern(77, 'z');
	const auto c = decode_entry(encode_entry(CommandChunk{9, CommandStream::upload, 1ull << 33, bytes}));
	REQUIRE(std::holds_alternative<CommandChunk>(c));
	CHECK_EQ(std::get<CommandChunk>(c).id, 9u);
	CHECK(std::get<CommandChunk>(c).stream == CommandStream::upload);
	CHECK_EQ(std::get<CommandChunk>(c).offset, 1ull << 33);
	CHECK(std::get<CommandChunk>(c).bytes == bytes);
}

// A command that fits is one entry, its encode_entry's bytes.
void test_fits_whole() {
	Command command;
	command.writes.push_back(write(CommandOp::replace_key, pattern(1000, 'a')));
	const auto entries = split_command(command, 5);
	REQUIRE_EQ(entries.size(), 1u);
	CHECK(entries[0] == encode_entry(command));
}

// A 3 MiB document with two blobs becomes chunks and a final entry, each within the 1 MiB limit with its header,
// and reassembles to the command.
void test_chunked_within_limit() {
	Command command;
	command.schema_version = 3;
	command.writes.push_back(write(CommandOp::replace_key, pattern(3 * 1024 * 1024 + 17, 'd'), {pattern(700 * 1024, 'b'), pattern(100, 'c')}));
	const std::uint64_t id = 0xdeadbeefcafeull;
	const auto entries = split_command(command, id);
	CHECK_GT(entries.size(), 4u);   // 3.7 MiB staged, at most 1 MiB a chunk
	for (const auto& entry : entries) {
		CHECK_LE(entry.size(), COMMAND_ENTRY_BYTES);
	}
	REQUIRE(std::holds_alternative<CommandFinal>(decode_entry(entries.back())));
	const auto& final = std::get<CommandFinal>(decode_entry(entries.back()));
	CHECK(final.command.writes[0].document.staged);
	CHECK(final.command.writes[0].blobs[0].record.staged);
	CHECK_FALSE(final.command.writes[0].blobs[1].record.staged);   // small bytes stay in place
	CHECK(reassemble(entries, id) == command);
}

// At limits around a command's size, every entry fits and the command reassembles.
void test_limits_around_the_size() {
	Command command;
	for (int i = 0; i < 3; ++i) {
		command.writes.push_back(write(CommandOp::replace_key, pattern(900 + static_cast<std::size_t>(i) * 400, 'a'), {pattern(500, 'b')}));
	}
	const auto whole = encode_entry(command).size();
	for (std::size_t limit = 200; limit <= whole + 2; limit += 37) {
		const auto entries = split_command(command, 300 + limit, limit);
		for (const auto& entry : entries) {
			CHECK_LE(entry.size(), limit);
		}
		CHECK_EQ(entries.size() == 1, limit >= whole);
		CHECK(reassemble(entries, 300 + limit) == command);
	}
}

// What cannot fit is refused, never proposed over the limit.
void test_refusals_over_the_limit() {
	Command big;
	big.writes.push_back(write(CommandOp::replace_key, pattern(2 * 1024 * 1024, 'a')));
	CHECK(raises(engine::errc::invalid, [&] { (void)encode_entry(big); }));
	CHECK(raises(engine::errc::invalid, [&] { (void)encode_entry(CommandFinal{1, big}); }));
	Command many;   // headers alone past the limit: nothing to stage
	for (int i = 0; i < 2000; ++i) {
		many.writes.push_back(write(CommandOp::erase_key, std::string()));
	}
	CHECK(raises(engine::errc::invalid, [&] { (void)split_command(many, 1, 4096); }));
}

// Every prefix of an entry, and entries with an unknown kind, version, op, flags, stream or trailing bytes, are
// refused as corrupt.
void test_corrupt_refused() {
	Command command;
	command.writes.push_back(write(CommandOp::replace_key, pattern(40, 'a'), {pattern(9, 'b')}));
	const auto entry = encode_entry(command);
	for (std::size_t n = 0; n < entry.size(); ++n) {
		CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(std::string_view(entry).substr(0, n)); }));
	}
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(entry + "x"); }));
	auto with = [&](std::size_t at, char byte) {
		auto bad = entry;
		bad[at] = byte;
		return bad;
	};
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(with(0, 9)); }));      // kind
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(with(1, 2)); }));      // version
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(with(4, 0)); }));      // op (after schema 0, count 1)
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(with(4, 7)); }));
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(with(5, 2)); }));      // flags
	auto chunk = encode_entry(CommandChunk{1, CommandStream::staging, 0, "abc"});
	chunk[3] = 2;                                                                      // stream
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(chunk); }));
	std::string huge_count = "\x01\x01\x00\xff\xff\xff\xff\x0f";                      // 2^32 writes in 8 bytes
	CHECK(raises(engine::errc::corrupt, [&] { (void)decode_entry(huge_count); }));
	// A staged range shorter than the stream holds is refused at resolve.
	Command staged;
	staged.writes.push_back(write(CommandOp::replace_key, std::string()));
	staged.writes[0].document = CommandBytes{{}, true, CommandStream::staging, 0, 10};
	CHECK(raises(engine::errc::corrupt, [&] { (void)resolve_command(staged, [](CommandStream, std::uint64_t, std::uint64_t) { return std::string("short"); }); }));
}

}  // namespace

TEST_MAIN(test_round_trip, test_fits_whole, test_chunked_within_limit, test_limits_around_the_size, test_refusals_over_the_limit, test_corrupt_refused)
