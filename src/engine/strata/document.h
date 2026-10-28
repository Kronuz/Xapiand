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

#pragma once

#include <cstddef>                 // for std::size_t
#ifdef ENGINE_CENSUS
#include <functional>              // for std::function
#endif
#include <map>                     // for std::map
#include <memory>                  // for std::unique_ptr
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector

#include "engine/document.h"       // for Document


// The strata implementation of the engine seam. Only files in this directory name strata or prospect.
namespace engine::strata {

// One term of a document: its within-document frequency and positions.
struct DocTerm {
	termcount wdf = 0;
	std::vector<termpos> positions;
};

// The document of the strata implementation: its terms, values, stored data and key, held in memory
// until the shard hands them to the index.
class StrataDocument final : public Document::Native {
public:
	void add_term(const Field& field, std::string_view term, termcount wdf_inc) override;
	void add_boolean_term(const Field& field, std::string_view term) override;
	void set_key(std::string_view key) override;
	void add_posting(const Field& field, std::string_view term, termpos position, termcount wdf_inc) override;
	void add_value(valueno slot, std::string value) override;
	std::string get_value(valueno slot) const override;
	void set_data(std::string data) override;
	std::string get_data() const override;
	std::string serialise() const override;
#ifdef ENGINE_CENSUS
	void census_terms(const std::function<void(std::string_view, termcount, std::size_t)>& visit) const override;
	std::size_t census_values() const override;
#endif

	// The term that names the document, empty when it has none.
	const std::string& key() const noexcept { return key_; }
	const std::string& data() const noexcept { return data_; }
	const std::map<std::string, DocTerm, std::less<>>& terms() const noexcept { return terms_; }
	const std::map<valueno, std::string>& values() const noexcept { return values_; }

	// The version this document was written at: the shard stamps it when it writes, the log keeps it,
	// and replay reads it back. 0 until written.
	rev version() const noexcept { return version_; }
	void stamp(rev version) noexcept { version_ = version; replayed_ = false; }
	// True for a document read from the log: the shard applies it only if the index is not already at
	// its version.
	bool replayed() const noexcept { return replayed_; }

	// Sorts and deduplicates every term's positions; the shard calls it before it reads them.
	void normalise();

	// Replaces this document's content with what `serialised` holds. Raises errc::syntax on bytes
	// that are not a serialised document.
	void unserialise_from(std::string_view serialised);

private:
	// Adds to a term by its whole bytes: the interim flat term (interim_terms.h), or one read from the log.
	DocTerm& flat(std::string_view term);

	std::map<std::string, DocTerm, std::less<>> terms_;
	std::map<valueno, std::string> values_;
	std::string data_;
	std::string key_;
	rev version_ = 0;
	bool replayed_ = false;
};

std::unique_ptr<Document::Native> new_native();

// The document the write-ahead log's bytes hold.
Document unserialise_document(std::string_view serialised);

// The strata document inside an engine document of this implementation.
StrataDocument& strata_document(Document& doc);
const StrataDocument& strata_document(const Document& doc);

}  // namespace engine::strata
