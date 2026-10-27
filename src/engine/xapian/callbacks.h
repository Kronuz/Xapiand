/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move

#include "engine/callbacks.h"      // for PostingSource, KeyMaker, MatchSpy
#include "xapian.h"                // for Xapian::ValuePostingSource, KeyMaker, MatchSpy, Registry


// Xapian's view of the host callbacks: each adapter is a Xapian base that forwards to an
// engine callback, so a Xapian matcher (and the remote server) calls Xapiand's code.
namespace engine::xapian {

// A Xapian value posting source that accepts the documents the engine source accepts.
class PostingSourceAdapter final : public Xapian::ValuePostingSource {
public:
	explicit PostingSourceAdapter(std::unique_ptr<engine::PostingSource> source);

	void next(double min_wt) override;
	void skip_to(Xapian::docid min_docid, double min_wt) override;
	bool check(Xapian::docid min_docid, double min_wt) override;
	double get_weight() const override;
	PostingSourceAdapter* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	PostingSourceAdapter* unserialise_with_registry(const std::string& serialised, const Xapian::Registry& registry) const override;
	void reset(const Xapian::Database& db, Xapian::doccount shard_index) override;
	std::string get_description() const override;

private:
	void set_bounds();

	std::unique_ptr<engine::PostingSource> source_;
};


class KeyMakerAdapter final : public Xapian::KeyMaker {
public:
	explicit KeyMakerAdapter(std::unique_ptr<engine::KeyMaker> key_maker) : key_maker_(std::move(key_maker)) { }

	// Borrows the key maker, which must outlive the adapter.
	explicit KeyMakerAdapter(const engine::KeyMaker& key_maker) : borrowed_(&key_maker) { }

	std::string operator()(const Xapian::Document& doc) const override;
	KeyMakerAdapter* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	KeyMakerAdapter* unserialise(const std::string& serialised, const Xapian::Registry& registry) const override;

private:
	const engine::KeyMaker& get() const noexcept {
		return key_maker_ ? *key_maker_ : *borrowed_;
	}

	std::unique_ptr<engine::KeyMaker> key_maker_;
	const engine::KeyMaker* borrowed_ = nullptr;
};


class MatchSpyAdapter final : public Xapian::MatchSpy {
public:
	explicit MatchSpyAdapter(std::unique_ptr<engine::MatchSpy> spy) : owned_(std::move(spy)), spy_(owned_.get()) { }

	// Borrows the spy, which must outlive the adapter (and holds the results after the match).
	explicit MatchSpyAdapter(engine::MatchSpy& spy) : spy_(&spy) { }

	void operator()(const Xapian::Document& doc, double wt) override;
	MatchSpyAdapter* clone() const override;
	std::string name() const override;
	std::string serialise() const override;
	MatchSpyAdapter* unserialise(const std::string& serialised, const Xapian::Registry& registry) const override;
	std::string serialise_results() const override;
	void merge_results(const std::string& serialised) override;
	std::string get_description() const override;

private:
	std::unique_ptr<engine::MatchSpy> owned_;
	engine::MatchSpy* spy_;
};


// The remote server rebuilds callbacks by name from a registry: these register a prototype.
inline void
register_posting_source(Xapian::Registry& registry, const engine::PostingSource& prototype)
{
	registry.register_posting_source(PostingSourceAdapter(prototype.clone()));
}

inline void
register_key_maker(Xapian::Registry& registry, const engine::KeyMaker& prototype)
{
	registry.register_key_maker(new KeyMakerAdapter(prototype.clone()));
}

inline void
register_match_spy(Xapian::Registry& registry, const engine::MatchSpy& prototype)
{
	registry.register_match_spy(MatchSpyAdapter(prototype.clone()));
}

}  // namespace engine::xapian
