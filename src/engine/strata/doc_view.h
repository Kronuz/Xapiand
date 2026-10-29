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

#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::pair
#include <vector>                  // for std::vector

#include "prospect/index.hh"       // for prospect::segment

#include "engine/doc_view.h"       // for DocView


namespace engine::strata {

// A document of a segment, read lazily: nothing is fetched until it is asked for. It borrows the
// segment, so it lives no longer than the pin (or the shard's lock) that keeps the segment open.
class SegmentDocView final : public DocView {
public:
	// `id` is the document's id as the caller numbers it (across shards, in a search).
	SegmentDocView(docid id, const prospect::segment& seg, prospect::ordinal_t ordinal) noexcept : id_(id), seg_(&seg), ordinal_(ordinal) { }

	docid did() const override { return id_; }
	std::string data() const override;
	std::string value(valueno slot) const override;
	rev version() const override;
	void values(const ValueSink& sink) const override;
	std::string serialise() const override;

private:
	docid id_;
	const prospect::segment* seg_;
	prospect::ordinal_t ordinal_;
};

// A document copied out of a segment, for a caller that outlives the pin: the shard's get and scan
// hand these out.
class OwnedDocView final : public DocView {
public:
	// Copies everything the view holds that is cheap to keep (data, version, values).
	explicit OwnedDocView(const SegmentDocView& view);

	docid did() const override { return id_; }
	std::string data() const override { return data_; }
	std::string value(valueno slot) const override;
	rev version() const override { return version_; }
	void values(const ValueSink& sink) const override;
	std::string serialise() const override;

private:
	docid id_;
	rev version_;
	std::string data_;
	std::vector<std::pair<valueno, std::string>> values_;
};

}  // namespace engine::strata
