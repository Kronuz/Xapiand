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

#include "engine/document.h"

#include "engine/error.h"          // for Error, errc
#include "engine/strata/document.h"  // for engine::strata::new_native, unserialise_document
#include "engine/xapian/convert.h" // for engine::xapian::new_native, unserialise_document


// A document is built in the representation of the engine that will write it (engine-write-path.md).

engine::Document::Document() : Document(default_kind()) { }


engine::Document::Document(Kind kind)
	: native_(kind == Kind::strata ? strata::new_native() : xapian::new_native()) { }


engine::FieldRef
engine::Document::field(std::string_view name, FieldKind kind)
{
	const auto it = field_ids_.find(name);
	if (it != field_ids_.end()) {
		auto& known = fields_[it->second];
		if (kind > known.kind) {
			known.kind = kind;
		}
		return FieldRef{it->second};
	}
	const auto id = static_cast<std::uint32_t>(fields_.size());
	fields_.push_back(Field{std::string(name), kind});
	field_ids_.emplace(fields_.back().name, id);
	return FieldRef{id};
}


engine::Document
engine::Document::unserialise(std::string_view serialised)
{
	return unserialise(default_kind(), serialised);
}


engine::Document
engine::Document::unserialise(Kind kind, std::string_view serialised)
{
	return kind == Kind::strata ? strata::unserialise_document(serialised) : xapian::unserialise_document(serialised);
}
