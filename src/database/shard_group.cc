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

#include "database/shard_group.h"

#include <stdexcept>               // for std::logic_error
#include <utility>                 // for std::move
#include <variant>                 // for std::get_if

#include "database/data.h"         // for Data, ct_type_t
#include "engine/document.h"       // for engine::Document
#include "engine/error.h"          // for engine::Error, engine::errc
#include "engine/strata/document.h"  // for engine::strata::StrataDocument
#include "engine/strata/shard.h"   // for engine::strata::append_write
#include "varint.hh"               // for varint::leb128


namespace {

namespace cs = kronuz::consensus;

constexpr char STATE_MAGIC[2] = {'X', 'G'};
constexpr std::uint8_t STATE_VERSION = 1;

engine::WriteOp
write_op(CommandOp op)
{
	switch (op) {
		case CommandOp::add: return engine::WriteOp::add;
		case CommandOp::replace: return engine::WriteOp::replace;
		case CommandOp::replace_key: return engine::WriteOp::replace_key;
		case CommandOp::erase: return engine::WriteOp::erase;
		case CommandOp::erase_key: return engine::WriteOp::erase_key;
		case CommandOp::set_metadata: break;
	}
	return engine::WriteOp::none;
}

// A chunk applies unless its id is open in a lower term: that command failed at this entry (D12).
bool
chunk_applies(const ShardGroupState& s, const CommandChunk& chunk, std::uint64_t term)
{
	const auto it = s.open.find(chunk.id);
	return it == s.open.end() || it->second.term >= term;
}

// The volume an upload chunk of `id` writes: its upload's, or the next number when this is its first.
std::uint64_t
upload_volume(const ShardGroupState& s, std::uint64_t id)
{
	const auto it = s.open.find(id);
	return it != s.open.end() && it->second.upload != 0 ? it->second.upload : s.next;
}

}  // namespace


std::string
volume_name(std::uint64_t volume)
{
	return "vol-" + std::to_string(volume);
}


std::string
staging_name(std::uint64_t id)
{
	return "stage-" + std::to_string(id);
}


BlobPlacement
place_blobs(const ShardGroupState& state, const Command& command, std::uint64_t id)
{
	BlobPlacement out;
	out.after = state;
	auto& s = out.after;
	out.at.reserve(command.writes.size());
	for (const auto& write : command.writes) {
		auto& at = out.at.emplace_back();
		at.reserve(write.blobs.size());
		for (const auto& blob : write.blobs) {
			const auto& record = blob.record;
			if (record.staged && record.stream == CommandStream::upload) {
				const auto it = state.open.find(id);
				if (it == state.open.end() || it->second.upload == 0) {
					throw engine::Error(engine::errc::corrupt, "A blob names an upload its command never streamed");
				}
				at.emplace_back(it->second.upload, record.offset);
				continue;
			}
			const auto size = record.size();
			if (s.length > 0 && s.length + size > VOLUME_BYTES) {
				out.sealed.push_back(s.volume);
				s.volume = s.next++;
				s.length = 0;
			}
			at.emplace_back(s.volume, s.length);
			s.length += size;
		}
	}
	return out;
}


void
ShardApplication::effects(const state& s, const strata::group::entry& e, strata::group::file_set& files)
{
	resolved_ = Resolved{e.index, std::nullopt, 0};
	// An open chunked command of a lower term fails at the first entry of a higher one (D12): its files retire.
	for (const auto& [id, open] : s.open) {
		if (open.term < e.term) {
			files.retire(staging_name(id), e.index);
			if (open.upload != 0) {
				files.retire(volume_name(open.upload), e.index);
			}
		}
	}
	if (e.kind != cs::EntryKind::Command) {
		return;
	}
	auto decoded = decode_entry(e.command);
	if (const auto* chunk = std::get_if<CommandChunk>(&decoded)) {
		if (chunk_applies(s, *chunk, e.term)) {
			const auto name = chunk->stream == CommandStream::staging ? staging_name(chunk->id) : volume_name(upload_volume(s, chunk->id));
			files.write(name, chunk->offset, chunk->bytes);
		}
		return;
	}
	std::uint64_t id = 0;
	Command command;
	if (auto* final = std::get_if<CommandFinal>(&decoded)) {
		const auto it = s.open.find(final->id);
		if (it == s.open.end() || it->second.term < e.term) {
			return;   // its chunks were retired, or never applied: the command fails
		}
		id = final->id;
		command = std::move(final->command);
		auto read_staged = [&](CommandBytes& part) {
			if (part.staged && part.stream == CommandStream::staging) {
				part.bytes = files.read(staging_name(id), part.offset, static_cast<std::size_t>(part.length));
				if (part.bytes.size() != part.length) {
					throw engine::Error(engine::errc::corrupt, "A chunked command's staging is shorter than its final entry names");
				}
				part.staged = false;
				part.offset = part.length = 0;
			}
		};
		for (auto& write : command.writes) {
			read_staged(write.document);
			for (auto& blob : write.blobs) {
				read_staged(blob.record);
			}
		}
	} else {
		command = std::move(std::get<Command>(decoded));
	}
	// Each in-place record at the offset the state names; a volume rotated out is sealed after its last record.
	const auto placement = place_blobs(s, command, id);
	for (std::size_t w = 0; w < command.writes.size(); ++w) {
		for (std::size_t b = 0; b < command.writes[w].blobs.size(); ++b) {
			const auto& record = command.writes[w].blobs[b].record;
			if (!record.staged && !record.bytes.empty()) {
				const auto [volume, offset] = placement.at[w][b];
				files.write(volume_name(volume), offset, record.bytes);
			}
		}
	}
	for (const auto volume : placement.sealed) {
		files.seal(volume_name(volume));
	}
	if (id != 0) {
		files.retire(staging_name(id), e.index);
		if (const auto upload = s.open.at(id).upload; upload != 0) {
			files.seal(volume_name(upload));   // complete: immutable from here
		}
	}
	resolved_.command = std::move(command);
	resolved_.id = id;
}


const ShardApplication::Resolved&
ShardApplication::resolved(const state&, const strata::group::entry& e) const
{
	if (resolved_.index != e.index) {
		throw std::logic_error("ShardApplication: an entry's transaction or advance before its effects");
	}
	return resolved_;
}


std::optional<prospect::transaction>
ShardApplication::transaction(const state& s, const strata::group::entry& e) const
{
	const auto& r = resolved(s, e);
	if (!r.command) {
		return std::nullopt;
	}
	const auto& command = *r.command;
	const auto placement = place_blobs(s, command, r.id);
	prospect::transaction t;
	for (std::size_t w = 0; w < command.writes.size(); ++w) {
		const auto& write = command.writes[w];
		if (write.document.staged) {
			throw engine::Error(engine::errc::corrupt, "A command's document is in an upload stream");
		}
		if (write.op == CommandOp::set_metadata) {
			auto& op = t.ops.emplace_back();
			op.type = write.document.bytes.empty() ? prospect::operation::delete_meta : prospect::operation::set_meta;
			op.meta_key = write.key;
			op.meta_value = write.document.bytes;
			continue;
		}
		engine::PreparedWrite prepared;
		prepared.op = write_op(write.op);
		prepared.did = write.did;
		prepared.key = write.key;
		prepared.expected = write.expected;
		prepared.result.did = write.did;
		if (write.document.bytes.empty()) {
			engine::strata::append_write(t, prepared, nullptr, index_->fields());
			continue;
		}
		auto doc = engine::Document::unserialize(write.document.bytes);
		if (!write.blobs.empty()) {
			// The blobs' locators, from the placement: a function of the log on every replica.
			Data data(doc.get_data());
			for (std::size_t b = 0; b < write.blobs.size(); ++b) {
				const auto& blob = write.blobs[b];
				const auto [volume, offset] = placement.at[w][b];
				data.update(ct_type_t(blob.ct_type), static_cast<ssize_t>(volume), static_cast<size_t>(offset), static_cast<size_t>(blob.record.size()));
			}
			data.flush();
			doc.set_data(data.serialize());
		}
		auto& native = dynamic_cast<engine::strata::StrataDocument&>(doc.native());
		prepared.result.version = native.version();
		engine::strata::append_write(t, prepared, &native, index_->fields());
	}
	if (t.ops.empty()) {
		return std::nullopt;
	}
	return t;
}


void
ShardApplication::advance(state& s, const strata::group::entry& e)
{
	const auto& r = resolved(s, e);
	std::optional<CommandChunk> chunk;
	if (e.kind == cs::EntryKind::Command && !r.command) {
		auto decoded = decode_entry(e.command);
		if (const auto* c = std::get_if<CommandChunk>(&decoded)) {
			if (chunk_applies(s, *c, e.term)) {
				chunk = *c;
			}
		}
	}
	if (r.command) {
		s = place_blobs(s, *r.command, r.id).after;
		if (r.id != 0) {
			s.open.erase(r.id);
		}
	}
	std::erase_if(s.open, [&](const auto& open) { return open.second.term < e.term; });
	if (chunk) {
		auto& open = s.open[chunk->id];
		open.term = e.term;
		if (chunk->stream == CommandStream::upload && open.upload == 0) {
			open.upload = s.next++;
		}
	}
}


std::string
ShardApplication::encode(const state& s) const
{
	std::string out(STATE_MAGIC, sizeof(STATE_MAGIC));
	out.push_back(static_cast<char>(STATE_VERSION));
	char buf[varint::leb128::max_bytes];
	auto put = [&](std::uint64_t value) { out.append(buf, varint::leb128::encode(buf, value)); };
	put(s.volume);
	put(s.length);
	put(s.next);
	put(s.open.size());
	for (const auto& [id, open] : s.open) {
		put(id);
		put(open.term);
		put(open.upload);
	}
	return out;
}


ShardApplication::state
ShardApplication::decode(std::string_view bytes) const
{
	auto bad = [] { throw engine::Error(engine::errc::corrupt, "Bad shard group state"); };
	if (bytes.size() < 3 || bytes[0] != STATE_MAGIC[0] || bytes[1] != STATE_MAGIC[1] || static_cast<std::uint8_t>(bytes[2]) != STATE_VERSION) {
		bad();
	}
	const char* p = bytes.data() + 3;
	const char* end = bytes.data() + bytes.size();
	auto get = [&] {
		std::uint64_t value = 0;
		if (!varint::leb128::decode(&p, end, value)) {
			bad();
		}
		return value;
	};
	state s;
	s.volume = get();
	s.length = get();
	s.next = get();
	for (auto count = get(); count > 0; --count) {
		const auto id = get();
		auto& open = s.open[id];
		open.term = get();
		open.upload = get();
	}
	if (p != end || s.volume == 0 || s.next <= s.volume) {
		bad();
	}
	return s;
}
