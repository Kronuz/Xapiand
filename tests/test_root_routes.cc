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

// The node's own routes (SearchViews): GET /, HEAD /, GET /:info, GET /:metrics and the selectors of GET / name
// no index, so they never build an Endpoint (the base itself is not an index and an Endpoint refuses it); any
// path with an index part does. node_info is what they answer.

#include "test_harness.h"

#include <string>
#include <string_view>

#include "msgpack.h"
#include "server/search_views.h"
#include "url_parser.h"

namespace {

bool node_route(std::string_view path) {
	PathParser parser;
	CHECK_LT(static_cast<int>(parser.init(path)), static_cast<int>(PathParser::State::END));
	return is_node_route(parser);
}

void test_the_root_and_its_commands_and_selectors_are_node_routes() {
	for (const char* path : {"/", "/:info", "/:metrics", "/.name", "/.server.name", "/.state"}) {
		CHECK_TRUE(node_route(path));
	}
}

void test_a_path_with_an_index_part_is_not() {
	for (const char* path : {"/idx", "/idx/", "/idx/1", "/idx/doc", "/idx/.name", "/idx/:info", "/idx/1:info", "/a/b/c",
	                         "/%2e%2e", "/%2Ftmp%2Fx"}) {
		CHECK_FALSE(node_route(path));
	}
}

void test_the_node_answers_with_its_server_versions_and_options() {
	auto info = node_info("");
	CHECK_TRUE(info.is_map());
	for (const char* key : {"name", "server", "versions", "options"}) {
		CHECK_TRUE(info.find(key) != info.end());
	}
	CHECK_EQ(info.at("server").at("name").str(), std::string("Xapiand"));
	CHECK_TRUE(info.at("versions").find("Xapiand") != info.at("versions").end());
	CHECK_TRUE(info.at("options").at("thread_pools").find("num_shards") != info.at("options").at("thread_pools").end());
	// A selector reduces the answer.
	CHECK_EQ(node_info("server.name").str(), std::string("Xapiand"));
}

}  // namespace

TEST_MAIN(
	test_the_root_and_its_commands_and_selectors_are_node_routes,
	test_a_path_with_an_index_part_is_not,
	test_the_node_answers_with_its_server_versions_and_options
)
