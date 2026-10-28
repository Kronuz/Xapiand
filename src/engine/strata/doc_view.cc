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

#include "engine/strata/doc_view.h"

#include <algorithm>               // for std::lower_bound

#include "engine/error.h"          // for Error, errc
#include "engine/strata/values.h"  // for values::Reader


namespace engine::strata {

namespace {

// The strata engine keeps no term lists (the owner's ruling R-P2): buckets come from stored values.
[[noreturn]] void
no_term_lists()
{
	throw Error(errc::syntax, "The strata engine keeps no term lists; a document's terms are not offered (count a keyword field's stored values instead)");
}

}  // namespace


std::string
SegmentDocView::data() const
{
	return seg_->get<::strata::stored_plugin>().stored(ordinal_);
}


std::string
SegmentDocView::value(valueno slot) const
{
	const auto bytes = values::Reader(*seg_).value(ordinal_, slot);
	return bytes ? std::string(*bytes) : std::string();
}


rev
SegmentDocView::version() const
{
	return seg_->get<::strata::keys_plugin>().version(ordinal_);
}


void
SegmentDocView::values(const ValueSink& sink) const
{
	values::Reader(*seg_).each(ordinal_, [&](std::uint32_t slot, std::string_view bytes) { sink(slot, bytes); });
}


void
SegmentDocView::terms(std::string_view, const TermSink&) const
{
	no_term_lists();
}


void
SegmentDocView::term_details(const TermDetailSink&) const
{
	no_term_lists();
}


std::string
SegmentDocView::serialise() const
{
	no_term_lists();
}


OwnedDocView::OwnedDocView(const SegmentDocView& view) : id_(view.did()), version_(view.version()), data_(view.data())
{
	view.values([&](valueno slot, std::string_view bytes) { values_.emplace_back(slot, std::string(bytes)); });
}


std::string
OwnedDocView::value(valueno slot) const
{
	const auto it = std::lower_bound(values_.begin(), values_.end(), slot, [](const auto& entry, valueno s) { return entry.first < s; });
	return it != values_.end() && it->first == slot ? it->second : std::string();
}


void
OwnedDocView::values(const ValueSink& sink) const
{
	for (const auto& [slot, bytes] : values_) {
		sink(slot, bytes);
	}
}


void
OwnedDocView::terms(std::string_view, const TermSink&) const
{
	no_term_lists();
}


void
OwnedDocView::term_details(const TermDetailSink&) const
{
	no_term_lists();
}


std::string
OwnedDocView::serialise() const
{
	no_term_lists();
}

}  // namespace engine::strata
