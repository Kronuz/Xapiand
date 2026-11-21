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

// A write as the group splits it (cluster-release D3): the leader prepares it against its index, and every voter
// applies the prepared write with the document's serialized bytes against its own index. A replica that applies
// the same prepared writes in the same order holds the same documents and field table, and an expectation that no
// longer holds is refused at apply, on every replica alike.

#include "test_harness.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "engine/error.h"
#include "engine/open.h"
#include "engine/strata/shard.h"
#include "engine/sync.h"

namespace {

std::string fresh(const char* name) {
	const auto path = (std::filesystem::current_path() / name).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(const std::string& key, const std::vector<std::string>& fields, const std::string& data) {
	engine::Document doc;
	doc.set_key(key);
	for (const auto& name : fields) {
		doc.add_term(doc.field(name), name + ":" + key);
	}
	doc.set_data(data);
	return doc;
}

struct Logged {
	engine::PreparedWrite prepared;
	std::string document;   // empty for an erase
};

// Prepares on the leader, applies there, and keeps what a command would carry.
Logged write(engine::Shard& leader, engine::WriteOp op, const std::string& key, engine::Document* doc, engine::WriteArgs args = {}) {
	Logged out;
	out.prepared = engine::sync_wait(leader.prepare(op, 0, key, doc, std::move(args)));
	if (doc) {
		out.document = doc->serialize();
	}
	engine::sync_wait(leader.apply(out.prepared, doc));
	return out;
}

// Applies on a voter from the command's bytes alone.
engine::WriteResult replay(engine::Shard& voter, const Logged& logged) {
	if (logged.document.empty()) {
		return engine::sync_wait(voter.apply(logged.prepared, nullptr));
	}
	auto doc = engine::Document::unserialize(logged.document);
	return engine::sync_wait(voter.apply(logged.prepared, &doc));
}

bool raises(engine::errc kind, const std::function<void()>& f) {
	try {
		f();
	} catch (const engine::Error& e) {
		return e.kind == kind;
	}
	return false;
}

std::vector<std::string> field_names(engine::Shard& shard) {
	std::vector<std::string> out;
	for (const auto& f : dynamic_cast<engine::strata::Shard&>(shard).declared_fields()) {
		out.push_back(f.name);
	}
	return out;
}

// A voter applying the leader's prepared writes holds what the leader holds: ids, versions, data, field table.
void test_voter_applies_the_prepared_writes() {
	const auto lpath = fresh("group_prepare_leader.db");
	const auto vpath = fresh("group_prepare_voter.db");
	auto leader = engine::sync_wait(engine::open_local(lpath, true, true));
	auto voter = engine::sync_wait(engine::open_local(vpath, true, true));
	std::vector<Logged> log;
	auto a = make("Ka", {"f", "g"}, "a1");
	log.push_back(write(*leader.shard, engine::WriteOp::replace_key, "Ka", &a));
	auto b = make("Kb", {"h", "f"}, "b1");
	log.push_back(write(*leader.shard, engine::WriteOp::add, "", &b));
	auto a2 = make("Ka", {"x", "g"}, "a2");
	log.push_back(write(*leader.shard, engine::WriteOp::replace_key, "Ka", &a2));
	log.push_back(write(*leader.shard, engine::WriteOp::erase_key, "Kb", nullptr));
	auto c = make("Kc", {"x"}, "c1");
	log.push_back(write(*leader.shard, engine::WriteOp::replace_key, "Kc", &c));

	CHECK_EQ(log[0].prepared.result.did, 1u);
	CHECK_EQ(log[2].prepared.result.version, 2u);
	CHECK(log[2].prepared.expected == std::optional<engine::rev>(1));
	CHECK(log[3].prepared.expected == std::optional<engine::rev>(1));
	for (const auto& logged : log) {
		const auto result = replay(*voter.shard, logged);
		CHECK_EQ(result.did, logged.prepared.result.did);
		CHECK_EQ(result.version, logged.prepared.result.version);
	}
	auto& l = *leader.shard;
	auto& v = *voter.shard;
	engine::sync_wait(l.commit(engine::Transaction::none));
	engine::sync_wait(v.commit(engine::Transaction::none));
	CHECK(field_names(l) == field_names(v));
	for (const auto* key : {"Ka", "Kb", "Kc"}) {
		CHECK_EQ(engine::sync_wait(l.find_key(key)), engine::sync_wait(v.find_key(key)));
	}
	CHECK_EQ(engine::sync_wait(v.find_key("Kb")), 0u);
	const auto ld = engine::sync_wait(l.get(1));
	const auto vd = engine::sync_wait(v.get(1));
	CHECK(ld->data() == vd->data());
	CHECK_EQ(vd->version(), 2u);
	CHECK_EQ(l.doc_count(), v.doc_count());
	leader.shard.reset();
	voter.shard.reset();
	std::filesystem::remove_all(lpath);
	std::filesystem::remove_all(vpath);
}

// Two writes prepared against the same state: the first applies, the second's expectation no longer holds and
// apply refuses it with version_conflict on every replica, leaving the document as the first left it.
void test_stale_expectation_refused_at_apply() {
	const auto lpath = fresh("group_prepare_stale_leader.db");
	const auto vpath = fresh("group_prepare_stale_voter.db");
	auto leader = engine::sync_wait(engine::open_local(lpath, true, true));
	auto voter = engine::sync_wait(engine::open_local(vpath, true, true));
	auto& l = *leader.shard;
	auto& v = *voter.shard;
	auto first = make("Ka", {"f"}, "v1");
	replay(v, write(l, engine::WriteOp::replace_key, "Ka", &first));

	auto one = make("Ka", {"f"}, "one");
	auto two = make("Ka", {"f"}, "two");
	Logged p1{engine::sync_wait(l.prepare(engine::WriteOp::replace_key, 0, "Ka", &one, {})), one.serialize()};
	Logged p2{engine::sync_wait(l.prepare(engine::WriteOp::replace_key, 0, "Ka", &two, {})), two.serialize()};
	CHECK(p1.prepared.expected == p2.prepared.expected);
	engine::sync_wait(l.apply(p1.prepared, &one));
	CHECK(raises(engine::errc::version_conflict, [&] { engine::sync_wait(l.apply(p2.prepared, &two)); }));
	replay(v, p1);
	CHECK(raises(engine::errc::version_conflict, [&] { replay(v, p2); }));
	engine::sync_wait(l.commit(engine::Transaction::none));
	engine::sync_wait(v.commit(engine::Transaction::none));
	CHECK(engine::sync_wait(l.get(1))->data() == engine::sync_wait(v.get(1))->data());
	CHECK_EQ(engine::sync_wait(v.get(1))->version(), 2u);

	// A conditional erase prepared before a replace of its key is refused too.
	Logged erase{engine::sync_wait(l.prepare(engine::WriteOp::erase_key, 0, "Ka", nullptr, {})), {}};
	auto three = make("Ka", {"f"}, "three");
	replay(v, write(l, engine::WriteOp::replace_key, "Ka", &three));
	CHECK(raises(engine::errc::version_conflict, [&] { engine::sync_wait(l.apply(erase.prepared, nullptr)); }));
	CHECK(raises(engine::errc::version_conflict, [&] { replay(v, erase); }));
	CHECK_EQ(engine::sync_wait(v.find_key("Ka")), 1u);
	leader.shard.reset();
	voter.shard.reset();
	std::filesystem::remove_all(lpath);
	std::filesystem::remove_all(vpath);
}

// Prepare changes nothing: a prepared write never applied leaves the index as it was.
void test_prepare_writes_nothing() {
	const auto path = fresh("group_prepare_nothing.db");
	auto opened = engine::sync_wait(engine::open_local(path, true, true));
	auto& s = *opened.shard;
	auto doc = make("Ka", {"f"}, "a");
	const auto p = engine::sync_wait(s.prepare(engine::WriteOp::replace_key, 0, "Ka", &doc, {}));
	CHECK_EQ(p.result.did, 1u);
	CHECK_EQ(engine::sync_wait(s.written_key("Ka")), 0u);
	CHECK_EQ(engine::sync_wait(s.written_count()), 0u);
	CHECK(raises(engine::errc::doc_not_found, [&] { (void)engine::sync_wait(s.prepare(engine::WriteOp::erase_key, 0, "Ka", nullptr, {})); }));
	opened.shard.reset();
	std::filesystem::remove_all(path);
}

}  // namespace

TEST_MAIN(test_voter_applies_the_prepared_writes, test_stale_expectation_refused_at_apply, test_prepare_writes_nothing)
