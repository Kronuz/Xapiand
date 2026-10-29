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
#include <cstdint>                 // for std::uint32_t
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

// One term of a document: its field, its bytes in the document's arena, its within-document frequency, and its
// positions as a chain through the document's position nodes, in arrival order.
struct DocTerm {
	static constexpr std::uint32_t none = 0xffffffffu;
	std::uint32_t field = 0;       // index into the document's fields
	std::uint32_t offset = 0;
	std::uint32_t size = 0;
	termcount wdf = 0;
	std::uint32_t hash = 0;
	std::uint32_t first = none;
	std::uint32_t last = none;
	std::uint32_t count = 0;
};

// A field of a document: its name, the widest kind the document asked for, and how many terms it has.
struct DocField {
	std::string name;
	FieldKind kind = FieldKind::boolean;
	std::uint32_t term_count = 0;
};

// The document of the strata implementation: its terms by field, values, stored data and key, held in
// memory until the shard hands them to the index.
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

	// The key that names the document, empty when it has none.
	const std::string& key() const noexcept { return key_; }
	// The frequencies the host gave the key field's terms: they count in the document's length but are
	// not postings (the key is the index's, not a term).
	termcount key_wdf() const noexcept { return key_wdf_; }
	const std::string& data() const noexcept { return data_; }
	// The fields that have terms, in the order they first arrived.
	const std::vector<DocField>& fields() const noexcept { return fields_; }
	// Every term of every field, in the order they first arrived.
	const std::vector<DocTerm>& terms() const noexcept { return terms_; }
	// The terms grouped by field: order[starts[f] .. starts[f + 1]) are the indexes into terms() of field f's, in the
	// order they arrived.
	struct Grouped {
		std::vector<std::uint32_t> order;
		std::vector<std::uint32_t> starts;
	};
	void group(Grouped& out) const;
	// The field of this name, or null.
	const DocField* field(std::string_view name) const noexcept;
	// The term under a field, or null.
	const DocTerm* find_term(const DocField& field, std::string_view bytes) const noexcept;
	const DocTerm* find_term(std::string_view field, std::string_view bytes) const noexcept;
	std::string_view term_bytes(const DocTerm& term) const noexcept { return std::string_view(bytes_).substr(term.offset, term.size); }
	// Replaces `out` with the term's positions, increasing and without repeats.
	void positions_into(const DocTerm& term, std::vector<termpos>& out) const;
	std::vector<termpos> positions_of(const DocTerm& term) const;
	const std::map<valueno, std::string>& values() const noexcept { return values_; }

	// The version this document was written at: the shard stamps it when it writes, the log keeps it,
	// and replay reads it back. 0 until written.
	rev version() const noexcept { return version_; }
	void stamp(rev version) noexcept { version_ = version; replayed_ = false; }
	// True for a document read from the log: the shard applies it only if the index is not already at
	// its version.
	bool replayed() const noexcept { return replayed_; }

	// Replaces this document's content with what `serialised` holds. Raises errc::syntax on bytes
	// that are not a serialised document.
	void unserialise_from(std::string_view serialised);

private:
	// The term under a field, created empty when absent; the field's kind widens to the call's.
	DocTerm& term_of(const Field& field, std::string_view term);
	std::size_t field_of(const Field& field);
	DocTerm& intern(std::size_t field, std::string_view bytes);
	void rebuild(std::size_t slots);
	void push_position(DocTerm& term, termpos position);

	std::vector<DocField> fields_;
	std::vector<DocTerm> terms_;
	std::vector<std::uint32_t> table_;  // open addressing over (field, bytes): a term's index plus one, 0 when empty
	std::string bytes_;                // the bytes of every term
	struct Node {
		termpos position;
		std::uint32_t next;
	};
	std::vector<Node> nodes_;          // the positions of every term, chained per term
	std::vector<std::uint32_t> slot_;  // the host's field id to its index in fields_ plus one, 0 when not seen
	std::map<valueno, std::string> values_;
	std::string data_;
	std::string key_;
	termcount key_wdf_ = 0;
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
