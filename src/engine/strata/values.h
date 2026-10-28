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

#include <cstdint>                 // for std::uint32_t
#include <optional>                // for std::optional
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move

#include "prospect/index.hh"       // for prospect::index
#include "prospect/operators.hh"   // for prospect::slot_values
#include "prospect/values.hh"      // for prospect::values_plugin

#include "engine/error.h"          // for Error, errc
#include "engine/strata/document.h"  // for StrataDocument
#include "engine/types.h"          // for valueno


// The value columns of the strata engine: the seam's opaque byte values, one single-valued bytes
// column per slot (xapiand-on-strata D3), from prospect's values plugin.
namespace engine::strata::values {

inline constexpr bool available = true;

// Copies a document's values into the index document. A document holds no empty value (setting one
// clears the slot, as Xapian's add_value does), and the plugin's reserved slot is Xapian's
// BAD_VALUENO, which no document carries.
inline void
put(prospect::index::document& out, const StrataDocument& doc)
{
	out.values.clear();
	for (const auto& [slot, bytes] : doc.values()) {
		if (slot == prospect::values_format::reserved_slot) {
			throw Error(errc::invalid, "The value slot is reserved");
		}
		out.values.push_back({slot, bytes});
	}
}

// The values of one segment: a document's value by slot, and a slot's column as a scan.
class Reader {
public:
	explicit Reader(const prospect::segment& seg) : reader_(&seg.get<prospect::values_plugin>()) { }

	std::optional<std::string_view> value(prospect::ordinal_t ord, valueno slot) const {
		return reader_->value(ord, slot);
	}

	// Calls sink(slot, bytes) for each value of the document, by slot.
	template <typename F>
	void each(prospect::ordinal_t ord, F&& sink) const {
		reader_->each_value(ord, std::forward<F>(sink));
	}

	// The slot's column, or null when no document of the segment holds a value in it.
	const prospect::values_plugin::column* column(valueno slot) const {
		return reader_->get(slot);
	}

private:
	const prospect::values_plugin::reader* reader_;
};

// A slot's column for prospect::source_scorer: the ordinals that hold a value, in order.
class Slot {
public:
	explicit Slot(const prospect::values_plugin::column& column) : column_(&column), cursor_(column) { }
	// A copy starts from the first document, which is what a scorer's fresh() needs.
	Slot(const Slot& other) : column_(other.column_), cursor_(*other.column_) { }
	Slot& operator=(const Slot&) = delete;

	prospect::ordinal_t next_present(prospect::ordinal_t from) const {
		return cursor_.advance(from);
	}
	std::string_view value(prospect::ordinal_t ord) const {
		if (const auto bytes = column_->value(ord)) {
			return *bytes;
		}
		return {};
	}
	std::uint64_t count() const {
		return column_->documents();
	}

private:
	const prospect::values_plugin::column* column_;
	mutable prospect::values_plugin::column::cursor cursor_;
};

}  // namespace engine::strata::values
