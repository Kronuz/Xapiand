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

#include "engine/xapian/callbacks.h"

#include "engine/census.h"             // for ENGINE_CENSUS_*
#include "engine/error.h"              // for Error
#include "engine/xapian/convert.h"     // for make_view


namespace engine::xapian {

namespace {

// The engine reports a malformed serialisation as invalid; Xapian's remote protocol as a network error.
template <typename F>
auto
rebuild(F&& f)
{
	try {
		return f();
	} catch (const Error& exc) {
		throw Xapian::NetworkError(exc.reason());
	}
}

}  // namespace


PostingSourceAdapter::PostingSourceAdapter(std::unique_ptr<engine::PostingSource> source)
	: Xapian::ValuePostingSource(source->slot()),
	  source_(std::move(source))
{
	set_bounds();
}


// Xapian resets the weight bound when it binds the source to a database, so it is set again then.
void
PostingSourceAdapter::set_bounds()
{
	if (const auto max_weight = source_->max_weight(); max_weight != 0.0) {
		set_maxweight(max_weight);
	}
}


void
PostingSourceAdapter::next(double min_wt)
{
	Xapian::ValuePostingSource::next(min_wt);
	while (!at_end()) {
		ENGINE_CENSUS_COUNT("callback.source:" + source_->name() + ".accept");
		if (source_->accept(get_value())) { break; }
		Xapian::ValuePostingSource::next(min_wt);
	}
}


void
PostingSourceAdapter::skip_to(Xapian::docid min_docid, double min_wt)
{
	Xapian::ValuePostingSource::skip_to(min_docid, min_wt);
	while (!at_end()) {
		ENGINE_CENSUS_COUNT("callback.source:" + source_->name() + ".accept");
		if (source_->accept(get_value())) { break; }
		Xapian::ValuePostingSource::next(min_wt);
	}
}


bool
PostingSourceAdapter::check(Xapian::docid min_docid, double min_wt)
{
	if (!Xapian::ValuePostingSource::check(min_docid, min_wt)) {
		// check returned false, so we know the document is not in the source.
		return false;
	}

	if (at_end()) {
		// return true, since we're definitely at the end of the list.
		return true;
	}

	ENGINE_CENSUS_COUNT("callback.source:" + source_->name() + ".accept");
	return source_->accept(get_value());
}


double
PostingSourceAdapter::get_weight() const
{
	ENGINE_CENSUS_COUNT("callback.source:" + source_->name() + ".weight");
	return source_->weight(get_value());
}


PostingSourceAdapter*
PostingSourceAdapter::clone() const
{
	return new PostingSourceAdapter(source_->clone());
}


std::string
PostingSourceAdapter::name() const
{
	return source_->name();
}


std::string
PostingSourceAdapter::serialise() const
{
	return source_->serialise();
}


PostingSourceAdapter*
PostingSourceAdapter::unserialise_with_registry(const std::string& serialised, const Xapian::Registry& /*registry*/) const
{
	return rebuild([&] { return new PostingSourceAdapter(source_->unserialise(serialised)); });
}


void
PostingSourceAdapter::reset(const Xapian::Database& db, Xapian::doccount shard_index)
{
	Xapian::ValuePostingSource::reset(db, shard_index);
	set_bounds();

	// A filter: possibly no document is accepted.
	set_termfreq_min(0);
}


std::string
PostingSourceAdapter::get_description() const
{
	return source_->description();
}


std::string
KeyMakerAdapter::operator()(const Xapian::Document& doc) const
{
	ENGINE_CENSUS_CALLBACK(census_scope, "keymaker", get().name());
	return get().key(*make_view(doc));
}


KeyMakerAdapter*
KeyMakerAdapter::clone() const
{
	return new KeyMakerAdapter(get().clone());
}


std::string
KeyMakerAdapter::name() const
{
	return get().name();
}


std::string
KeyMakerAdapter::serialise() const
{
	return get().serialise();
}


KeyMakerAdapter*
KeyMakerAdapter::unserialise(const std::string& serialised, const Xapian::Registry& /*registry*/) const
{
	return rebuild([&] { return new KeyMakerAdapter(get().unserialise(serialised)); });
}


void
MatchSpyAdapter::operator()(const Xapian::Document& doc, double wt)
{
	ENGINE_CENSUS_CALLBACK(census_scope, "spy", spy_->name());
	spy_->see(*make_view(doc), wt);
}


MatchSpyAdapter*
MatchSpyAdapter::clone() const
{
	return new MatchSpyAdapter(spy_->clone());
}


std::string
MatchSpyAdapter::name() const
{
	return spy_->name();
}


std::string
MatchSpyAdapter::serialise() const
{
	return spy_->serialise();
}


MatchSpyAdapter*
MatchSpyAdapter::unserialise(const std::string& serialised, const Xapian::Registry& /*registry*/) const
{
	return rebuild([&] { return new MatchSpyAdapter(spy_->unserialise(serialised)); });
}


std::string
MatchSpyAdapter::serialise_results() const
{
	return spy_->serialise_results();
}


void
MatchSpyAdapter::merge_results(const std::string& serialised)
{
	spy_->merge_results(serialised);
}


std::string
MatchSpyAdapter::get_description() const
{
	return spy_->description();
}

}  // namespace engine::xapian
