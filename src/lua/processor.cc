/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Processor itself and its cache: this thread's compiled scripts, and compiling local and foreign ones.

#include "processor.h"
#include "processor_detail.h"

#if XAPIAND_LUA

#include <algorithm>                              // for std::min
#include <atomic>                                 // for std::atomic
#include <cassert>
#include <chrono>                                 // for std::chrono::steady_clock
#include <cstdint>                                // for int64_t
#include <cstdlib>                                // for realloc, free
#include <exception>                              // for std::exception_ptr
#include <functional>                             // for std::hash
#include <new>                                    // for std::bad_alloc
#include <optional>                               // for std::optional
#include <string>
#include <string_view>
#include <thread>                                 // for std::this_thread
#include <utility>                                // for std::exchange

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "lua/exception.h"                        // for lua::ScriptLimitError
#include "database/handler.h"                     // for DatabaseHandler
#include "engine/error.h"                         // for engine::Error
#include "task.hh"                                // for task::task
#include "xapiand_exception.h"                    // for ClientError, THROW
#include "log.h"                                  // for L_ERR, L_INFO
#include "lru.h"                                  // for lru::lru
#include "manager.h"                              // for XapiandManager::*
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr
#include "server/search_service.h"                 // for BUFFERED_BODY_CAP
#include "url_parser.h"                           // for urldecode

namespace lua {

namespace {

// The object of the document a script's foreign link names.
task::task<MsgPack>
read_foreign_script(std::string foreign_path, std::string id)
{
	Endpoint endpoint(foreign_path);
	auto endpoints = co_await XapiandManager::resolve_index_endpoints(endpoint);
	if (endpoints.empty()) {
		THROW(ClientError, "Cannot resolve endpoint: {}", endpoint.to_string());
	}
	DatabaseHandler _db_handler(endpoints, DB_OPEN);
	auto doc = co_await _db_handler.get_document(id);
	co_return co_await doc.get_obj();
}


// This thread's compiled scripts.
class ScriptLRU : public lru::lru<std::string, std::shared_ptr<Processor>> {
public:
	explicit ScriptLRU(ssize_t max_size) :
		lru::lru(max_size) { }
};

// A local script is cached under its name, a foreign one under its reference: the first character keeps a
// client-chosen name from ever being a foreign key.
std::string local_key(std::string_view name) {
	return 'L' + std::string(name);
}

std::string foreign_key(std::string_view reference) {
	return 'F' + std::string(reference);
}

std::atomic<std::int64_t> foreign_age_ms{1000};

ScriptLRU& cache() {
	thread_local ScriptLRU script_lru(opts.scripts_cache_size);
	return script_lru;
}

// What a thread's cached scripts may hold: one call's cap.
constexpr std::size_t cache_budget = Limits{}.max_bytes;

// A call that grew its state by this much collects it afterwards; a smaller one leaves its garbage to the next
// call's collection, so a trivial call (a few KiB) costs no collection. It is what the cache charges each state
// beyond its settled bytes: 100 ordinary scripts are under 9 MiB of the 64 MiB budget.
constexpr std::size_t collect_after = 64u << 10;

// A script up to this size compiles in about a millisecond (a table constructor, the slowest to compile, takes
// about 53 ms a MiB), so a call that overlaps another call of it compiles a state of its own; a larger script's
// calls take the cached state in turn.
constexpr std::size_t own_state_source = 16u << 10;

std::size_t cached_bytes(ScriptLRU& lru) {
	std::size_t total = 0;
	for (const auto& entry : lru) {
		total += entry.second->resident_bytes();
	}
	return total;
}

// Drops the least recently used scripts past the count or past the byte budget.
void trim(ScriptLRU& lru = cache()) {
	std::size_t total = cached_bytes(lru);
	while (!lru.empty() && (lru.size() > lru.max_size() || total > cache_budget)) {
		const std::string oldest = lru.begin()->first;
		total -= lru.begin()->second->resident_bytes();
		lru.erase(oldest);
	}
}

std::shared_ptr<Processor> cached(const std::string& name, std::string_view body) {
	auto& lru = cache();
	auto it = lru.find(name);
	if (it != lru.end()) {
		if (body.empty() || it->second->get_hash() == std::hash<std::string_view>()(body)) {
			return it->second;
		}
	}
	return nullptr;
}

std::shared_ptr<Processor> remember(const std::string& name, std::shared_ptr<Processor> processor) {
	L_INFO("Script {} ({:x}) compiled and ready.", repr(name), processor->get_hash());
	auto& lru = cache();
	auto inserted = lru.emplace(name, processor);
	if (!inserted.second) {
		inserted.first->second = processor;  // the script changed under its name
	}
	trim(lru);
	return processor;
}

}  // namespace


static constexpr std::string_view prefix = "local _ENV = ...; ";

std::string_view
Processor::body() const
{
	return std::string_view(source).substr(prefix.size());
}


Processor::Processor(std::string_view name, std::string_view body, const MsgPack& params, Limits limits_) :
	hash(std::hash<std::string_view>()(body)),
	script_name(name),
	limits(limits_),
	owner(std::this_thread::get_id())
{
	// The body on the line of `local _ENV`, so line numbers hold.
	source = prefix;
	source.append(body);
	if (params.is_map() && params.size() != 0) {
		stored_params = params.serialize();
	}
	resident = std::make_unique<detail::State>(script_name, source, limits.max_bytes);
}


Processor::~Processor() = default;


std::size_t
Processor::resident_bytes() const
{
	return resident->settled + collect_after + source.size() + stored_params.size();
}


task::task<void>
Processor::operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params, task::budget* budget)
{
	assert(owner == std::this_thread::get_id());  // a Processor is its thread's own

	// A call is user code that can hold the reactor for a stretch no hook interrupts (a library call), so it runs in the
	// heavy lane from its first turn and leaves it on exit (docs/design/loop-scheduling.md D7). It holds nothing yet.
	const bool light = task::current() && !task::current().heavy();
	if (light) {
		co_await task::schedule_on{task::current().with_heavy(true)};
	}
	struct LightOnExit {
		bool light;
		~LightOnExit() {
			if (light) { task::enter_light(); }
		}
	} light_on_exit{light};

	// One call per state: the cached state when it is free; else a small script's call compiles one of its own
	// and a large script's call waits for the cached state.
	task::async_mutex::guard held;
	const task::cancel_token token = budget != nullptr ? budget->token() : task::cancel_token{};
	if (source.size() > own_state_source) {
		held = co_await resident_lock.lock(token);
		if (!held) {
			throw task::cancelled{};
		}
	} else if (auto take = resident_lock.lock(task::cancel_token{}); take.await_ready()) {
		held = take.await_resume();   // free: taken without waiting
	}
	std::unique_ptr<detail::State> own;
	detail::State* state = resident.get();
	if (!held) {
		own = std::make_unique<detail::State>(script_name, source, limits.max_bytes);
		state = own.get();
	}

	// A call that grew its state collects it however it ended (a result, an error, a bound, a cancellation).
	std::exception_ptr error;
	try {
		co_await run(*state, limits, method, doc, old_doc, stored_params, params, budget);
	} catch (...) {
		error = std::current_exception();
	}
	assert(owner == std::this_thread::get_id());
	if (state->used >= state->settled + collect_after) {
		co_await collect(*state, budget);
		if (state == resident.get()) {
			trim();   // the cache charges this state anew
		}
	}
	if (error) {
		std::rethrow_exception(error);
	}
}


std::shared_ptr<Processor>
Processor::compile_local(const Script& script)
{
	auto name_body = script.get_name_body();
	const auto key = local_key(name_body.first);
	if (auto processor = cached(key, name_body.second)) {
		return processor;
	}
	if (name_body.second.empty()) {
		THROW(ClientError, "Script {} has no body and is not cached; send its '_body'", repr(name_body.first));
	}
	return remember(key, std::make_shared<Processor>(name_body.first, name_body.second, MsgPack::MAP()));   // its params come with each call
}


std::chrono::milliseconds
Processor::foreign_revalidation()
{
	return std::chrono::milliseconds(foreign_age_ms.load(std::memory_order_relaxed));
}


void
Processor::set_foreign_revalidation(std::chrono::milliseconds age)
{
	foreign_age_ms.store(age.count(), std::memory_order_relaxed);
}


std::shared_ptr<Processor>
Processor::cached_local(const std::string& name)
{
	return cached(local_key(name), {});
}


std::shared_ptr<Processor>
Processor::cached_foreign(const std::string& reference)
{
	return cached(foreign_key(reference), {});
}


bool
Processor::foreign_trusted(const std::string& reference)
{
	auto hit = cached(foreign_key(reference), {});
	return hit && std::chrono::steady_clock::now() - hit->checked < foreign_revalidation();
}


std::size_t
Processor::printed_bytes()
{
	return printed;
}


std::size_t
Processor::cached_bytes()
{
	return lua::cached_bytes(cache());
}


std::size_t
Processor::cache_budget()
{
	return lua::cache_budget;
}


void
Processor::remember_foreign(const std::string& reference, std::shared_ptr<Processor> processor)
{
	processor->checked = std::chrono::steady_clock::now();
	remember(foreign_key(reference), std::move(processor));
}


task::task<std::shared_ptr<Processor>>
Processor::compile(const Script& script)
{
	if (script.get_types()[SPC_FOREIGN_TYPE] != FieldType::foreign) {
		co_return compile_local(script);
	}

	const auto key = foreign_key(script.get_endpoint());
	if (foreign_trusted(std::string(script.get_endpoint()))) {
		co_return cached(key, {});
	}

	std::string_view foreign_path_view, foreign_id_view;
	split_path_id(script.get_endpoint(), foreign_path_view, foreign_id_view);
	std::string foreign_path = urldecode(foreign_path_view);
	std::string foreign_id = urldecode(foreign_id_view);
	MsgPack obj;
	try {
		obj = co_await read_foreign_script(foreign_path, foreign_id);
	} catch (const engine::Error& error) {
		switch (error.kind) {
			case engine::errc::doc_not_found: {
				THROW(ClientError, "Foreign script {}/{} doesn't exist", foreign_path, foreign_id);
				break;
			}
			case engine::errc::not_found: {
				THROW(ClientError, "Foreign script database {} doesn't exist", repr(foreign_path));
				break;
			}
			default:
				throw;
		}
	}
	// If there's no selector use "script" (to be consistent with "schema" field):
	obj = obj[SCRIPT_FIELD_NAME];
	Script foreign_script(obj);
	if (foreign_script.get_types()[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		THROW(ClientError, "Nested foreign scripts not supported!");
	}
	auto name_body = foreign_script.get_name_body();
	const MsgPack& params = foreign_script.get_params();   // the caller's come with each call

	// The read may have moved this task to another thread: the processor is built where it will run.
	// The cached one stands while the document's script and params are what it was built from.
	if (auto current = cached(key, {})) {
		const bool same_params = params.is_map() && params.size() != 0 ? current->stored_params == params.serialize() : current->stored_params.empty();
		if (current->get_hash() == std::hash<std::string_view>()(name_body.second) && same_params) {
			current->checked = std::chrono::steady_clock::now();
			co_return current;
		}
	}
	auto processor = std::make_shared<Processor>(name_body.first, name_body.second, params);
	processor->checked = std::chrono::steady_clock::now();
	co_return remember(key, std::move(processor));
}

}; // End namespace lua

#endif
