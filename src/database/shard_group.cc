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

#include <algorithm>               // for std::max
#include <chrono>                  // for std::chrono
#include <condition_variable>      // for std::condition_variable
#include <cstring>                 // for std::memcpy
#include <filesystem>              // for std::filesystem::create_directories
#include <random>                  // for std::random_device
#include <span>                    // for std::span
#include <stdexcept>               // for std::logic_error
#include <utility>                 // for std::move
#include <variant>                 // for std::get_if

#include "completion/posix.h"      // for kronuz::journal::PosixIO
#include "database/data.h"         // for Data, ct_type_t
#include "database/utils.h"        // for DB_SLOT_VERSION
#include "engine/document.h"       // for engine::Document
#include "engine/error.h"          // for engine::Error, engine::errc
#include "engine/strata/document.h"  // for engine::strata::StrataDocument
#include "engine/strata/shard.h"   // for engine::strata::append_write
#include "reserved/fields.h"       // for VERSION_FIELD_NAME
#include "serialize.h"             // for Serialize::version
#include "varint.hh"               // for varint::leb128


namespace {

namespace cs = kronuz::consensus;

constexpr char STATE_MAGIC[2] = {'X', 'G'};
constexpr std::uint8_t STATE_VERSION = 2;

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

// A chunk or final entry applies only in the term its id names (D12): placed in another, a leader change cut its
// command, whose files a higher term's first entry retired.
bool
in_its_term(std::uint64_t id, std::uint64_t term)
{
	return chunked_term(id) == term;
}

// Whether a blob of `command` is in its upload stream.
bool
names_upload(const Command& command)
{
	for (const auto& write : command.writes) {
		for (const auto& blob : write.blobs) {
			if (blob.record.staged && blob.record.stream == CommandStream::upload) {
				return true;
			}
		}
	}
	return false;
}

// The volume an upload chunk of `id` writes: its upload's, or the next number when this is its first.
std::uint64_t
upload_volume(const ShardGroupState& s, std::uint64_t id)
{
	const auto it = s.open.find(id);
	return it != s.open.end() && it->second.upload != 0 ? it->second.upload : s.next;
}

}  // namespace


CommandWrite
command_write(const engine::PreparedWrite& prepared, engine::Document* doc)
{
	CommandWrite w;
	switch (prepared.op) {
		case engine::WriteOp::add: w.op = CommandOp::add; break;
		case engine::WriteOp::replace: w.op = CommandOp::replace; break;
		case engine::WriteOp::replace_key: w.op = CommandOp::replace_key; break;
		case engine::WriteOp::erase: w.op = CommandOp::erase; break;
		case engine::WriteOp::erase_key: w.op = CommandOp::erase_key; break;
		case engine::WriteOp::none: throw std::logic_error("command_write: a write with nothing to apply");
	}
	w.did = prepared.did;
	w.key = prepared.key;
	w.expected = prepared.expected;
	if (doc != nullptr) {
		w.document.bytes = doc->serialize();
	}
	return w;
}


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


std::string
place_stored(std::string bytes, const std::vector<StoredPlace>& places)
{
	if (bytes.empty() || places.empty()) {
		return bytes;
	}
	Data data(std::move(bytes));
	for (const auto& place : places) {
		data.update(ct_type_t(place.ct_type), static_cast<ssize_t>(place.volume), static_cast<size_t>(place.offset), static_cast<size_t>(place.size));
	}
	data.flush();
	// A flush leaves its locators viewing the bytes it replaced: the codecs are set on the new bytes, parsed again.
	// The locators just set come last, so a codec set in place moves no other locator's bytes.
	Data placed(std::string(data.serialize()));
	bool typed = false;
	for (auto& locator : placed) {
		for (const auto& place : places) {
			const auto type = static_cast<Locator::Type>(place.type);
			if (type != Locator::Type::stored && locator.ct_type.to_string() == place.ct_type) {
				locator.type = type;
				typed = true;
			}
		}
	}
	if (typed) {
		placed.flush();
	}
	return placed.serialize();
}


std::uint8_t
stored_type(std::string_view bytes, std::string_view ct_type)
{
	const Data data{std::string(bytes)};
	for (const auto& locator : data) {
		if (locator.ct_type.to_string() == ct_type) {
			return static_cast<std::uint8_t>(locator.type);
		}
	}
	return static_cast<std::uint8_t>(Locator::Type::stored);
}


std::string
read_stored(kronuz::journal::IO& directory, std::uint8_t type, std::uint64_t volume, std::uint64_t offset, std::uint64_t size)
{
	auto file = directory.open_existing_read_only(std::string(strata::group::bundle_detail::prefix) + volume_name(volume));
	std::string record(static_cast<std::size_t>(size), '\0');
	for (std::size_t done = 0; done < record.size();) {
		const auto got = file->read_at(offset + done, std::span<char>(record.data() + done, record.size() - done));
		if (got == 0) {
			throw engine::Error(engine::errc::corrupt, "A stored blob's record ends before its locator's size");
		}
		done += got;
	}
	if (static_cast<Locator::Type>(type) != Locator::Type::zstd_compressed_stored) {
		return record;
	}
	Locator locator{ct_type_t()};
	locator.type = Locator::Type::zstd_compressed_stored;
	locator.raw = record;
	locator.size = record.size();
	return std::string(locator.data());
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
		if (chunked_term(id) < e.term) {
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
		if (in_its_term(chunk->id, e.term)) {
			const auto name = chunk->stream == CommandStream::staging ? staging_name(chunk->id) : volume_name(upload_volume(s, chunk->id));
			files.write(name, chunk->offset, chunk->bytes);
		}
		return;
	}
	std::uint64_t id = 0;
	Command command;
	if (auto* final = std::get_if<CommandFinal>(&decoded)) {
		const auto it = s.open.find(final->id);
		if (it == s.open.end() || !in_its_term(final->id, e.term)) {
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
			// Complete and named: immutable from here. Named by no blob (an abort): its space is freed.
			if (names_upload(command)) {
				files.seal(volume_name(upload));
			} else {
				files.retire(volume_name(upload), e.index);
			}
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
	std::map<std::string, engine::rev, std::less<>> written;   // a key's version after this command's writes so far
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
			if (write.op == CommandOp::erase_key) {
				written[write.key] = 0;
			}
			engine::strata::append_write(t, prepared, nullptr, index_->fields());
			continue;
		}
		auto doc = engine::Document::unserialize(write.document.bytes);
		auto& native = dynamic_cast<engine::strata::StrataDocument&>(doc.native());
		// The version a replace by key lands at. An unconditional one (a RESTORE's, D17) was prepared ahead of the
		// entries before it: the key's version here, after this command's earlier writes, decides, as strata's does.
		std::optional<engine::rev> stamp;
		if (write.op == CommandOp::replace_key) {
			engine::rev landing = 1;
			if (write.expected) {
				landing = *write.expected + 1;
			} else if (const auto it = written.find(write.key); it != written.end()) {
				landing = it->second + 1;
			} else if (const auto found = index_->find_key(write.key)) {
				landing = static_cast<engine::rev>(found->second) + 1;
			}
			written[write.key] = landing;
			if (landing != native.version()) {
				stamp = landing;
				native.stamp(landing);
				native.add_value(DB_SLOT_VERSION, Serialize::version(landing));
			}
		}
		if (!write.blobs.empty() || stamp) {
			auto bytes = doc.get_data();
			if (!write.blobs.empty()) {
				// The blobs' locators, from the placement: a function of the log on every replica.
				std::vector<StoredPlace> places;
				places.reserve(write.blobs.size());
				for (std::size_t b = 0; b < write.blobs.size(); ++b) {
					const auto& blob = write.blobs[b];
					const auto [volume, offset] = placement.at[w][b];
					places.push_back({blob.ct_type, stored_type(bytes, blob.ct_type), volume, offset, blob.record.size()});
				}
				bytes = place_stored(std::move(bytes), places);
			}
			if (stamp) {
				Data data(std::move(bytes));
				auto obj = data.get_obj();
				if (auto it = obj.find(VERSION_FIELD_NAME); it != obj.end()) {
					it.value() = *stamp;
					data.set_obj(obj);
				}
				data.flush();
				bytes = data.serialize();
			}
			doc.set_data(std::move(bytes));
		}
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
			if (in_its_term(c->id, e.term)) {
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
	std::erase_if(s.open, [&](const auto& open) { return chunked_term(open.first) < e.term; });
	if (chunk) {
		auto& open = s.open[chunk->id];
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
		s.open[id].upload = get();
	}
	if (p != end || s.volume == 0 || s.next <= s.volume) {
		bad();
	}
	return s;
}


/*
 * The process's journal: the host, its loop and the chore threads.
 */

namespace {

namespace jn = kronuz::journal;

// A lone voter (D2): this node, the group's only voter, with the D12 frame for its entries.
kronuz::consensus::GroupOptions
lone_voter_options()
{
	kronuz::consensus::GroupOptions o;
	o.configuration.local = 1;
	o.configuration.voters = {1};
	o.configuration.cluster[0] = 'X';
	o.configuration.configuration[0] = 'G';
	o.admission = jn::AdmissionLimits{{1ull << 30, 1u << 20}, {1u << 20, 16}, {96ull << 20, 64}, 64, 3};
	o.limits.command_bytes = COMMAND_ENTRY_BYTES;
	o.limits.rpc_bytes = 2 * COMMAND_ENTRY_BYTES;
	o.limits.log_bytes = 16 * COMMAND_ENTRY_BYTES;
	o.timing = kronuz::consensus::Timing{20, 40, 100, 200, 60000};
	return o;
}

}  // namespace


void
ShardGroups::Driven::begin()
{
	thread = std::thread([this] { scheduler.drive([this] { return quit.load(); }, [] { return true; }); });
}


void
ShardGroups::Driven::wake()
{
	scheduler.post_call([](void*) { }, nullptr);
}


void
ShardGroups::Driven::end()
{
	quit.store(true);
	wake();
	if (thread.joinable()) {
		thread.join();
	}
}


ShardGroups::ShardGroups(const std::string& directory, std::size_t chores)
{
	std::filesystem::create_directories(directory);
	host_ = std::make_unique<jn::Host>(std::make_shared<jn::PosixIO>(directory), jn::Identity{'X', 'H'}, jn::HostOptions{1, jn::TurnLogOptions{}});
	loop_ = std::make_unique<strata::group::loop>(loop_thread_.ref(), jn::completion_ref::of(io_), *host_, 0, std::random_device{}());
	task::spawn(loop_thread_.ref(), host_->driver(0).run(jn::completion_ref::of(io_), stop_.token()), [this](std::exception_ptr) { turns_done_.store(true); });
	task::spawn(loop_thread_.ref(), loop_->run(stop_.token()), [this](std::exception_ptr) { driver_done_.store(true); });
	for (std::size_t i = 0; i < std::max<std::size_t>(1, chores); ++i) {
		chores_.push_back(std::make_unique<Driven>());
	}
	loop_thread_.begin();
	for (auto& chore : chores_) {
		chore->begin();
	}
}


ShardGroups::~ShardGroups()
{
	stop_.request(task::cancel_reason::shutdown);
	loop_thread_.wake();
	while (!(turns_done_.load() && driver_done_.load())) {
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}
	for (auto& chore : chores_) {
		chore->end();
	}
	loop_thread_.end();
	loop_.reset();
	host_.reset();
}


task::scheduler_ref
ShardGroups::chore(kronuz::consensus::GroupId id) noexcept
{
	return chores_[static_cast<std::size_t>(id % chores_.size())]->ref();
}


kronuz::consensus::GroupId
ShardGroups::group_id(const std::array<char, 16>& uuid) noexcept
{
	std::uint64_t id = 0;
	std::memcpy(&id, uuid.data(), sizeof(id));
	return id != 0 ? id : 1;
}


ShardGroups::group_type::config
ShardGroups::config(const std::array<char, 16>& uuid)
{
	group_type::config c;
	c.id = group_id(uuid);
	c.options = lone_voter_options();
	c.identity = uuid;
	std::lock_guard<std::mutex> lk(closed_mtx_);
	if (auto it = closed_.find(c.id); it != closed_.end()) {
		c.closed = std::move(it->second);
		closed_.erase(it);
	}
	return c;
}


void
ShardGroups::closed(kronuz::consensus::GroupId id, std::optional<std::string> record)
{
	std::lock_guard<std::mutex> lk(closed_mtx_);
	if (record) {
		closed_[id] = std::move(*record);
	} else {
		closed_.erase(id);
	}
}


namespace {

// Runs `work` on `scheduler` and blocks the calling thread until it ends, rethrowing its error.
void
run_blocking(task::scheduler_ref scheduler, task::task<void> work)
{
	std::mutex mtx;
	std::condition_variable cv;
	bool done = false;
	std::exception_ptr error;
	task::spawn(scheduler, std::move(work), [&](std::exception_ptr e) {
		std::lock_guard<std::mutex> lk(mtx);
		error = e;
		done = true;
		cv.notify_one();   // under the lock: the waiter's frame outlives it
	});
	std::unique_lock<std::mutex> lk(mtx);
	cv.wait(lk, [&] { return done; });
	if (error) {
		std::rethrow_exception(error);
	}
}

task::task<void>
read_caught_up(ShardGroup& shard_group, const std::function<void(const ShardGroupState&, prospect::index&)>& at)
{
	auto& group = shard_group.group;
	co_await group.read_applied([&](prospect::index& index) {
		if (at) {
			at(group.state(), index);   // on the chore, between two entries
		}
		return true;
	});
}

task::task<void>
propose_aborts(ShardGroup& shard_group, std::vector<std::uint64_t> ids)
{
	for (const auto id : ids) {
		try {
			co_await shard_group.group.propose(encode_entry(CommandFinal{id, Command{}}), strata::group::ack::none);
		} catch (...) {
			// Not placed: the next term's first entry retires the command instead.
		}
	}
}

}  // namespace


void
ShardGroup::catch_up(const std::function<void(const ShardGroupState&, prospect::index&)>& at)
{
	run_blocking(group.owner().scheduler(), read_caught_up(*this, at));
}


void
ShardGroup::abort_open(const std::vector<std::uint64_t>& ids)
{
	if (!ids.empty()) {
		run_blocking(group.owner().scheduler(), propose_aborts(*this, ids));
	}
}


task::task<void>
checkpoint(ShardGroup& shard_group, std::function<void(prospect::index&)> at)
{
	const auto home = task::current();
	co_await task::schedule_on{shard_group.group.owner().scheduler()};
	std::exception_ptr error;
	try {
		const auto published = co_await shard_group.group.checkpoint();
		shard_group.checkpointed.store(published.through, std::memory_order_release);
		if (at) {
			co_await shard_group.group.read_applied([&](prospect::index& index) { at(index); });
		}
	} catch (const kronuz::consensus::closed&) {
		error = std::make_exception_ptr(engine::Error(engine::errc::not_available, "The shard's group is closing"));
	} catch (...) {
		error = std::current_exception();
	}
	co_await task::schedule_on{home};
	if (error) {
		std::rethrow_exception(error);
	}
}


ShardGroup::ShardGroup(ShardGroups& groups, kronuz::journal::IO& directory_, prospect::index& index) :
	app(index),
	group(groups.loop(), groups.config(index.manifest().uuid), directory_, index, app, groups.chore(ShardGroups::group_id(index.manifest().uuid))),
	directory(&directory_)
{
}


task::task<strata::group::answer>
propose_entry(ShardGroup& shard_group, std::string entry, strata::group::ack level)
{
	const auto home = task::current();
	co_await task::schedule_on{shard_group.group.owner().scheduler()};
	std::exception_ptr error;
	std::optional<strata::group::answer> out;
	try {
		out.emplace(co_await shard_group.group.propose(std::move(entry), level));
	} catch (const kronuz::consensus::closed&) {
		error = std::make_exception_ptr(engine::Error(engine::errc::not_available, "The shard's group is closing"));
	} catch (...) {
		error = std::current_exception();
	}
	co_await task::schedule_on{home};
	if (error) {
		std::rethrow_exception(error);
	}
	co_return std::move(*out);
}


task::task<std::uint64_t>
next_chunked_id(ShardGroup& shard_group)
{
	const auto home = task::current();
	auto& group = shard_group.group;
	co_await task::schedule_on{group.owner().scheduler()};
	std::uint64_t id = 0;
	if (const auto state = group.owner().driver().state(group.id())) {
		if (!shard_group.ids || shard_group.ids->term() != state->term) {
			shard_group.ids.emplace(state->term, std::max(state->committed, state->applied));
		}
		id = shard_group.ids->next();
	}
	co_await task::schedule_on{home};
	if (id == 0) {
		throw engine::Error(engine::errc::not_available, "The shard's group is not open");
	}
	co_return id;
}
