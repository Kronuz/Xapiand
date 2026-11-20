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


// The server's helpers that need no node: the mime table's parser and lookups, the content types a preview renders and
// the ids a GET reads as a range. The views themselves are driven by harness/coverage_e2e_server.py.

#include "test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <unistd.h>

#include "database/data.h"
#include "server/mime_types.h"

bool is_range(std::string_view str);
bool can_preview(const ct_type_t& ct_type);

namespace {

// The table is read once per process; this file has every shape the reader tells apart: words before `types` and before
// `{`, comments, a value list across lines, a closing `}` in the middle of a value, and words after the end.
constexpr const char* TABLE =
	"stray\n"
	"types stray2\n"
	"{\n"
	"  # a comment\n"
	"  text/plain  txt TEXT;\n"
	"  application/foo  foo\n"
	"       bar;\n"
	"  image/x-odd }\n"
	"       odd;\n"
	"  image/png png; }\n"
	"  after the end\n";

void test_mime_table() {
	const auto dir = std::filesystem::temp_directory_path() / ("xapiand_cov_mime_" + std::to_string(::getpid()));
	std::filesystem::create_directories(dir);
	{
		std::ofstream(dir / "mime.types") << TABLE;
	}
	::setenv("XAPIAND_DATA_PATH", dir.c_str(), 1);
	preload_mime_types();

	CHECK_EQ(mime_type("txt").to_string(), std::string("text/plain"));
	CHECK_EQ(mime_type("text").to_string(), std::string("text/plain"));   // the table is read lowercase
	CHECK_EQ(mime_type("note.TXT").to_string(), std::string("text/plain"));   // the last dot names the extension
	CHECK_EQ(mime_type("a.b.png").to_string(), std::string("image/png"));
	CHECK_EQ(mime_type(".png").to_string(), std::string("image/png"));
	CHECK_EQ(mime_type("foo").to_string(), std::string("application/foo"));   // a list continues on the next line
	CHECK_EQ(mime_type("bar").to_string(), std::string("application/foo"));
	CHECK_EQ(mime_type("odd").to_string(), std::string("image/x-odd"));       // the `}` inside a value is skipped
	CHECK(mime_type("after").empty());                                          // words after the closing `}` are ignored
	CHECK(mime_type("noextension.").empty());
	CHECK(mime_type("").empty());
	CHECK(mime_type("unknown").empty());

	std::filesystem::remove_all(dir);
}

void test_can_preview() {
	for (const char* ct : { "application/pdf", "application/eps", "application/postscript", "application/x-bzpdf",
	                        "application/x-eps", "application/x-gzpdf", "application/x-pdf", "application/x-photoshop",
	                        "application/photoshop", "application/psd", "image/png", "image/jpeg", "image/x-anything" }) {
		CHECK_TRUE(can_preview(ct_type_t(ct)));
	}
	for (const char* ct : { "application/json", "text/plain", "application/octet-stream", "video/mp4", "application/zip" }) {
		CHECK_FALSE(can_preview(ct_type_t(ct)));
	}
	CHECK_FALSE(can_preview(ct_type_t()));
}

void test_is_range() {
	CHECK_TRUE(is_range("[10,20]"));
	CHECK_TRUE(is_range("(10,20)"));
	CHECK_TRUE(is_range("(10,20]"));
	CHECK_TRUE(is_range("[10,20)"));
	CHECK_FALSE(is_range("d1"));
	CHECK_FALSE(is_range("plain-id"));
	CHECK_FALSE(is_range("[10"));          // a parse error is not a range
	CHECK_FALSE(is_range("[10,20"));
	CHECK_FALSE(is_range(""));
}

}  // namespace

TEST_MAIN(test_mime_table, test_can_preview, test_is_range)
