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

#include <cstdint>                 // for std::uint32_t, std::uint64_t
#include <string>                  // for std::string

// The engine seam: Xapiand-owned types and contracts that every storage engine
// implements. Nothing here includes an engine's headers.

namespace engine {

using docid = std::uint32_t;       // document id within one shard
using doccount = std::uint32_t;    // a number of documents
using valueno = std::uint32_t;     // value slot number
using termcount = std::uint32_t;   // within-document frequency, and counts of terms
using termpos = std::uint32_t;     // term position within a document
using rev = std::uint64_t;         // database revision, and document version

// What a write reports: the document's id in the shard, its new version, and its key term.
struct WriteResult {
	docid did = 0;
	rev version = 0;
	std::string term;
};

inline constexpr valueno BAD_VALUENO = static_cast<valueno>(-1);

// Open actions: the low bits of a shard's open flags (the other flags live in database/flags.h).
inline constexpr int DB_CREATE_OR_OPEN      = 0x00;  // Create the database if it doesn't already exist.
inline constexpr int DB_CREATE_OR_OVERWRITE = 0x01;  // Create the database, overwriting it if it exists.
inline constexpr int DB_CREATE              = 0x02;  // Create a new database.
inline constexpr int DB_OPEN                = 0x03;  // Open an existing database.
inline constexpr int DB_ACTION_MASK         = 0x03;

}  // namespace engine
