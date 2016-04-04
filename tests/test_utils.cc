/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_utils.h"

#include "../src/utils.h"

#include "log.h"


static std::string
run_url_path(const std::string& path, bool clear_id)
{
	const char* parser_url_path_states_names[] = {
		[PathParser::start] = "start",
		[PathParser::pmt] = "pmt",
		[PathParser::cmd] = "cmd",
		[PathParser::id] = "id",
		[PathParser::nsp] = "nsp",
		[PathParser::pth] = "pth",
		[PathParser::hst] = "hst",
		[PathParser::end] = "end",
		[PathParser::INVALID_STATE] = "INVALID_STATE",
		[PathParser::INVALID_NSP] = "INVALID_NSP",
		[PathParser::INVALID_HST] = "INVALID_HST",
	};

	PathParser::State state;
	std::string result;
	PathParser p;

	if ((state = p.init(path)) < PathParser::end) {
		result += "_|";
		if (clear_id) {
			p.off_id = nullptr;
		}
		if (p.off_cmd) {
			result += "cmd:" + std::string(p.off_cmd, p.len_cmd) + "|";
		}
		if (p.off_pmt) {
			result += "pmt:" + std::string(p.off_pmt, p.len_pmt) + "|";
		}
		if (p.off_id) {
			result += "id:" + std::string(p.off_id, p.len_id) + "|";
		}
	}

	while ((state = p.next()) < PathParser::end) {
		result += "_|";
		if (p.off_hst) {
			result += "hst:" + std::string(p.off_hst, p.len_hst) + "|";
		}
		if (p.off_nsp) {
			result += "nsp:" + std::string(p.off_nsp, p.len_nsp) + "|";
		}
		if (p.off_pth) {
			result += "pth:" + std::string(p.off_pth, p.len_pth) + "|";
		}
	}
	result += "(" + std::string(parser_url_path_states_names[state]) + ")";
	return result;
}


struct Url {
	std::string path;
	bool clear_id;
	std::string expected;
};


int test_url_path()
{
	std::vector<Url> urls = {
		{ "/namespace:path1/index1@host1,path2/index2@host2,path3/index3/search", false, "_|id:search|_|hst:host1|nsp:/namespace|pth:path1/index1|_|hst:host2|nsp:/namespace|pth:path2/index2|_|nsp:/namespace|pth:path3/index3|(end)" },
		{ "/namespace1:path1/index1@host1,path2/index2@host2,/namespace2:path3/index3/1/_cmd", false, "_|cmd:_cmd|id:1|_|hst:host1|nsp:/namespace1|pth:path1/index1|_|hst:host2|nsp:/namespace1|pth:path2/index2|_|nsp:/namespace2|pth:path3/index3|(end)" },
		{ "db_first.db,db_second.db/1/_search", false, "_|cmd:_search|id:1|_|pth:db_first.db|_|pth:db_second.db|(end)" },
		{ "db_first.db,db_second.db/_search", false, "_|cmd:_search|_|pth:db_first.db|_|pth:db_second.db|(end)" },
		{ "/path/subpath/1", false, "_|id:1|_|pth:/path/subpath|(end)" },
		{ "/database/", false, "_|id:database|_|pth:|(end)" },
		{ "path/1", false, "_|id:1|_|pth:path|(end)" },
		{ "/db_titles/localhost/_upload/", false, "_|cmd:_upload|id:localhost|_|pth:/db_titles|(end)" },
		{ "//path/to:namespace1/index1@host1,//namespace2/index2@host2:8890,namespace3/index3@host3/type1,type2/search////", false, "_|id:search|_|hst:host1|nsp://path/to|pth:namespace1/index1|_|hst:host2:8890|nsp://path/to|pth://namespace2/index2|_|hst:host3/type1|nsp://path/to|pth:namespace3/index3|_|nsp://path/to|pth:type2|(end)" },
		{ "/path/to:namespace1/index1@host1,/namespace2/index2@host2,namespace3/index3@host3/t1/_upload/search/", false, "_|cmd:_upload|pmt:search|id:t1|_|hst:host1|nsp:/path/to|pth:namespace1/index1|_|hst:host2|nsp:/path/to|pth:/namespace2/index2|_|hst:host3|nsp:/path/to|pth:namespace3/index3|(end)" },
		{ "/database.db/subdir/_upload/3/", true, "_|cmd:_upload|pmt:3|_|pth:/database.db/subdir|(end)" },
		{ "usr/dir:subdir,/_upload/1", false, "_|cmd:_upload|pmt:1|_|nsp:usr/dir|pth:subdir|_|nsp:usr/dir|pth:|(end)" },
		{ "/database.db/_upload/_search/", false, "_|cmd:_search|id:_upload|_|pth:/database.db|(end)" },
		{ "delete", false, "_|id:delete|_|pth:|(end)" },
		{ "delete", true, "_|_|pth:delete|(end)" },
		{ "/_stats/", false, "_|cmd:_stats|_|pth:|(end)" },
		{ "/index/_stats", false, "_|cmd:_stats|id:index|_|pth:|(end)" },
		{ "/index/_stats/1", false, "_|cmd:_stats|pmt:1|id:index|_|pth:|(end)" },
		{ "/index/1/_stats", false, "_|cmd:_stats|id:1|_|pth:/index|(end)" },
		{ "/_stats/", true, "_|cmd:_stats|_|pth:|(end)" },
		{ "/index/_stats", true, "_|cmd:_stats|_|pth:/index|(end)" },
		{ "/index/_stats/1", true, "_|cmd:_stats|pmt:1|_|pth:/index|(end)" },
		{ "/index/1/_stats", true, "_|cmd:_stats|_|pth:/index/1|(end)" },
		{ "/AQjN/BVf/78w/QjNBVfWKH78w/clients/clients.client.cd7ec34a-5d4a-11e5-b0b2-34363bc9ddd6/", false, "_|id:clients.client.cd7ec34a-5d4a-11e5-b0b2-34363bc9ddd6|_|pth:/AQjN/BVf/78w/QjNBVfWKH78w/clients|(end)" },
		{ "/favicon.ico", false, "_|id:favicon.ico|_|pth:|(end)" },
		{ "path/1,path/2,path/3/ID/_search", false, "_|cmd:_search|id:ID|_|pth:path/1|_|pth:path/2|_|pth:path/3|(end)" },
		{ ",path/1,path/2,path/3/ID/_search", false, "_|cmd:_search|id:ID|_|pth:|_|pth:path/1|_|pth:path/2|_|pth:path/3|(end)" },
		{ "path/1,,path/2,path/3/ID/_search", false, "_|cmd:_search|id:ID|_|pth:path/1|_|pth:|_|pth:path/2|_|pth:path/3|(end)" },
		{ "path/1,path/2,,path/3/ID/_search", false, "_|cmd:_search|id:ID|_|pth:path/1|_|pth:path/2|_|pth:|_|pth:path/3|(end)" },
		{ "path/1,path/2,path/3,/ID/_search", false, "_|cmd:_search|id:ID|_|pth:path/1|_|pth:path/2|_|pth:path/3|_|pth:|(end)" },
		{ "1", false, "_|id:1|_|pth:|(end)" },
		{ "1/", false, "_|id:1|_|pth:|(end)" },
		{ "/1", false, "_|id:1|_|pth:|(end)" },
		{ "/1/", false, "_|id:1|_|pth:|(end)" },
		{ "1,2", false, "_|id:1,2|_|pth:|(end)" },
		{ "1/,2/", false, "_|id:,2|_|pth:1|(end)" },
		{ "/1,/2", false, "_|id:2|_|pth:/1|_|pth:|(end)" },
		{ "1,2", true, "_|_|pth:1|_|pth:2|(end)" },
		{ "1/,2/", true, "_|_|pth:1/|_|pth:2/|(end)" },
		{ "/1,/2", true, "_|_|pth:/1|_|pth:/2|(end)" },
	};

	int count = 0;
	for (auto& url : urls) {
		std::string result = run_url_path(url.path, url.clear_id);
		if (result != url.expected) {
			L_ERR(nullptr, "\nError: the value obtained from the url path:\n%s (%s) should be:\n  %s\nbut it is:\n  %s", url.path.c_str(), url.clear_id ? "true" : "false", url.expected.c_str(), result.c_str());
			++count;
		}
	}

	return count;
}
