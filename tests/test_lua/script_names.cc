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

// Script names: local names, foreign references and links, and name-only requests.

#include "support.h"
#include "tests.h"

namespace detail {

// --- X4-S4: local names and foreign references are different keys ---------------------------------------

void test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference() {
	// The foreign key is a database path and a document id; a client can name a local script the same.
	const std::string reference = "scripts/shared";
	MsgPack local_object({{"_name", reference}, {"_body", "_doc.who = 'attacker'"}});
	Script local_script(local_object);
	auto foreign = std::make_shared<Processor>("scripts/shared", "_doc.who = 'victim'", MsgPack::MAP());
	Processor::remember_foreign(reference, foreign);
	auto local = Processor::compile_local(local_script);           // saved under the same name afterwards...
	CHECK(Processor::cached_foreign(reference) == foreign);        // ...the foreign entry stays
	CHECK(Processor::cached_local(reference) == local);
	CHECK(local != foreign);
	// The other order: the local script is cached first.
	const std::string reference2 = "scripts/other";
	MsgPack local_object2({{"_name", reference2}, {"_body", "_doc.who = 'attacker'"}});
	Script local_script2(local_object2);
	auto local2 = Processor::compile_local(local_script2);
	CHECK(Processor::cached_foreign(reference2) == nullptr);       // a foreign reference misses
	auto foreign2 = std::make_shared<Processor>("scripts/other", "_doc.who = 'victim'", MsgPack::MAP());
	Processor::remember_foreign(reference2, foreign2);
	CHECK(Processor::cached_local(reference2) == local2);
	CHECK(Processor::cached_foreign(reference2) == foreign2);
	CHECK_EQ(call(*Processor::cached_foreign(reference2), MsgPack::MAP()).doc.at("who").str(), "victim");
}

// --- S4: a cached foreign script is trusted for a second -----------------------------------------------

void test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again() {
	using namespace std::chrono_literals;
	CHECK(Processor::foreign_revalidation() == 1000ms);
	const std::string reference = "scripts/trusted";
	auto cached = std::make_shared<Processor>("scripts/trusted", "_doc.v = 1", MsgPack::MAP());
	Processor::remember_foreign(reference, cached);
	CHECK(Processor::foreign_trusted(reference));
	// Inside the window `compile` serves the cached processor without reading the document.
	MsgPack object({{"_type", "foreign/object"}, {"_foreign", reference}});
	Script foreign_script(object);
	CHECK(task::sync_wait(Processor::compile(foreign_script)) == cached);
	// Past it the entry is stale, so `compile` would read the document again.
	Processor::set_foreign_revalidation(20ms);
	std::this_thread::sleep_for(60ms);
	CHECK(!Processor::foreign_trusted(reference));
	Processor::remember_foreign(reference, cached);        // a read stamps the entry anew
	CHECK(Processor::foreign_trusted(reference));
	Processor::set_foreign_revalidation(1000ms);
	CHECK(!Processor::foreign_trusted("scripts/never-seen"));
}

// --- SR1: a foreign link is an index path, and an index path names a place inside the data directory ----------

void test_a_foreign_link_that_leaves_the_data_directory_is_refused() {
	Endpoint::cwd = "/var/db/xapiand/";
	for (const char* reference : {"..%00/scripts/doc", "%2Fetc/scripts/doc", "%2e%2e/doc", "scripts%5cx/doc"}) {
		MsgPack object({{"_type", "foreign/object"}, {"_foreign", reference}});
		Script foreign_script(object);
		bool refused = false;
		try {
			task::sync_wait(Processor::compile(foreign_script));
		} catch (const ClientError&) {
			refused = true;
		}
		CHECK(refused);
	}
}

void test_a_name_only_request_for_a_script_that_is_not_cached_is_refused() {
	// Evicted by the count bound, and dropped for passing the byte budget: both answer an error naming the script.
	const auto saved = opts.scripts_cache_size;
	std::thread([saved] {
		auto name_only_refused = [](const std::string& name) {
			MsgPack object({{"_name", name}});
			Script script(object);
			MsgPack doc = doc_of({{"x", 1}});
			try {
				auto processor = Processor::compile_local(script);
				call(*processor, doc);
			} catch (const ClientError& e) {
				const bool is_400 = dynamic_cast<const PayloadTooLarge*>(&e) == nullptr;   // a plain ClientError answers 400
				return is_400 && doc == doc_of({{"x", 1}}) && std::string(e.what()).find(name) != std::string::npos;
			}
			return false;
		};
		auto upload = [](const std::string& name, const std::string& body) {
			MsgPack object({{"_name", name}, {"_body", body}});
			Script script(object);
			return Processor::compile_local(script);
		};
		opts.scripts_cache_size = 1;
		upload("miss-evicted-a", "_doc.ran = 1");
		upload("miss-evicted-b", "_doc.ran = 1");
		CHECK(Processor::cached_local("miss-evicted-a") == nullptr);
		CHECK(name_only_refused("miss-evicted-a"));
		opts.scripts_cache_size = saved;
		std::string big = "local function f() return {";
		for (int i = 0; big.size() < (9u << 20); ++i) {
			big += "'k" + std::to_string(i) + "',";
		}
		big += "} end _doc.ran = 1";
		upload("miss-big", big);
		CHECK(Processor::cached_local("miss-big") == nullptr);   // its charge alone passes the budget
		CHECK(name_only_refused("miss-big"));
		upload("miss-kept", "_doc.ran = 1");
		MsgPack object({{"_name", "miss-kept"}});
		Script script(object);
		CHECK(Processor::compile_local(script) != nullptr);   // a cached script still runs by name
	}).join();
	opts.scripts_cache_size = saved;
}

void test_a_schema_stores_the_body_of_a_name_only_script_or_refuses_it() {
	const auto saved = opts.scripts_cache_size;
	std::thread([] {
		auto declare = [](const std::string& name) {
			MsgPack object({{"_name", name}});
			Script script(object);
			return script.process_script(false);
		};
		MsgPack full({{"_name", "decl-cached"}, {"_body", "_doc.ran = _doc.x + 1"}});
		Script cached_script(full);
		Processor::compile_local(cached_script);
		// Cached: the stored script carries the body.
		auto stored = declare("decl-cached");
		CHECK_EQ(stored.at("_lua").at("_body").str(), "_doc.ran = _doc.x + 1");
		// Not cached: refused at declaration, with a plain ClientError (400).
		bool refused = false;
		try {
			declare("decl-missing");
		} catch (const ClientError& e) {
			refused = dynamic_cast<const PayloadTooLarge*>(&e) == nullptr && std::string(e.what()).find("decl-missing") != std::string::npos;
		}
		CHECK(refused);
		// A write through the stored schema on a fresh thread (its own, empty cache) still runs the script.
		std::thread([stored] {
			CHECK(Processor::cached_local("decl-cached") == nullptr);
			Script from_schema(stored.at("_lua"));
			auto processor = Processor::compile_local(from_schema);
			auto out = call(*processor, doc_of({{"x", 41}}));
			CHECK(out.ok());
			CHECK_EQ(out.doc.at("ran").i64(), 42);
		}).join();
	}).join();
	opts.scripts_cache_size = saved;
}

}  // namespace detail
