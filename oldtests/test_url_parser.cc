/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_url_parser.h"

#include <string>
#include <vector>

#include "../src/url_parser.h"
#include "utils.h"


static std::string run_url_path(const std::string& path, bool clear_id) {
	const char* parser_url_path_states_names[] = {
		"slc", "slb", "ncm", "pmt", "cmd", "id", "nsp", "pth", "hst", "end", "INVALID_STATE", "INVALID_NSP", "INVALID_HST",
	};

	PathParser::State state;
	std::string result;
	PathParser p;

	if ((state = p.init(path)) < PathParser::State::END) {
		result += "_|";
		if (clear_id) {
			p.off_id = nullptr;
		}
		if (p.off_slc) {
			result += "slc=" + std::string(p.off_slc, p.len_slc) + "|";
		}
		if (p.off_cmd) {
			result += "cmd=" + std::string(p.off_cmd, p.len_cmd) + "|";
		}
		if (p.off_pmt) {
			result += "pmt=" + std::string(p.off_pmt, p.len_pmt) + "|";
		}
		if (p.off_ppmt) {
			result += "ppmt=" + std::string(p.off_ppmt, p.len_ppmt) + "|";
		}
		if (p.off_id) {
			result += "id=" + std::string(p.off_id, p.len_id) + "|";
		}
	}

	while ((state = p.next()) < PathParser::State::END) {
		result += "_|";
		if (p.off_hst) {
			result += "hst=" + std::string(p.off_hst, p.len_hst) + "|";
		}
		if (p.off_nsp) {
			result += "nsp=" + std::string(p.off_nsp, p.len_nsp) + "|";
		}
		if (p.off_pth) {
			result += "pth=" + std::string(p.off_pth, p.len_pth) + "|";
		}
	}
	result += "(" + std::string(parser_url_path_states_names[toUType(state)]) + ")";
	return result;
}


struct Url {
	std::string path;
	bool clear_id;
	std::string expected;
};


int test_url_path() {
	INIT_LOG
	std::vector<Url> urls = {
		{ "/namespace:path1/index1@host1,path2/index2@host2,path3/index3/search", false, "_|id=search|_|hst=host1|nsp=/namespace|pth=path1/index1|_|hst=host2|nsp=/namespace|pth=path2/index2|_|nsp=/namespace|pth=path3/index3|(end)" },
		{ "/namespace1:path1/index1@host1,path2/index2@host2,/namespace2:path3/index3/1/:cmd", false, "_|cmd=:cmd|id=1|_|hst=host1|nsp=/namespace1|pth=path1/index1|_|hst=host2|nsp=/namespace1|pth=path2/index2|_|nsp=/namespace2|pth=path3/index3|(end)" },
		{ "db_first.db,db_second.db/1/:search", false, "_|cmd=:search|id=1|_|pth=db_first.db|_|pth=db_second.db|(end)" },
		{ "db_first.db,db_second.db/:search", false, "_|cmd=:search|_|pth=db_first.db|_|pth=db_second.db|(end)" },
		{ "/path/subpath/1", false, "_|id=1|_|pth=/path/subpath|(end)" },
		{ "/database/", false, "_|id=database|_|pth=|(end)" },
		{ "path/1", false, "_|id=1|_|pth=path|(end)" },
		{ "/db_titles/localhost/:upload/", false, "_|cmd=:upload|id=localhost|_|pth=/db_titles|(end)" },
		{ "//path/to:namespace1/index1@host1,//namespace2/index2@host2:9880,namespace3/index3@host3/type1,type2/search////", false, "_|id=search|_|hst=host1|nsp=//path/to|pth=namespace1/index1|_|hst=host2:9880|nsp=//path/to|pth=//namespace2/index2|_|hst=host3/type1|nsp=//path/to|pth=namespace3/index3|_|nsp=//path/to|pth=type2|(end)" },
		{ "/path/to:namespace1/index1@host1,/namespace2/index2@host2,namespace3/index3@host3/t1/:upload/search/", false, "_|cmd=:upload|pmt=search|id=t1|_|hst=host1|nsp=/path/to|pth=namespace1/index1|_|hst=host2|nsp=/path/to|pth=/namespace2/index2|_|hst=host3|nsp=/path/to|pth=namespace3/index3|(end)" },
		{ "/database.db/subdir/:upload/3/", true, "_|cmd=:upload|pmt=3|_|pth=/database.db/subdir|(end)" },
		{ "usr/dir:subdir,/:upload/1", false, "_|cmd=:upload|pmt=1|_|nsp=usr/dir|pth=subdir|_|nsp=usr/dir|pth=|(end)" },
		{ "/database.db/:upload/:search/", false, "_|cmd=:search|id=:upload|_|pth=/database.db|(end)" },
		{ "delete", false, "_|id=delete|_|pth=|(end)" },
		{ "delete", true, "_|_|pth=delete|(end)" },
		{ "/:stats/", false, "_|cmd=:stats|_|pth=|(end)" },
		{ "/index/:stats", false, "_|cmd=:stats|id=index|_|pth=|(end)" },
		{ "/index/:stats/1", false, "_|cmd=:stats|pmt=1|id=index|_|pth=|(end)" },
		{ "/index/:stats/1/2/3", false, "_|cmd=:stats|pmt=1|ppmt=2/3|id=index|_|pth=|(end)" },
		{ "/index/1/:stats", false, "_|cmd=:stats|id=1|_|pth=/index|(end)" },
		{ "/:stats/", true, "_|cmd=:stats|_|pth=|(end)" },
		{ "/index/:stats", true, "_|cmd=:stats|_|pth=/index|(end)" },
		{ "/index/:stats/1", true, "_|cmd=:stats|pmt=1|_|pth=/index|(end)" },
		{ "/index/1/:stats", true, "_|cmd=:stats|_|pth=/index/1|(end)" },
		{ "/AQjN/BVf/78w/QjNBVfWKH78w/clients/clients.client.cd7ec34a-5d4a-11e5-b0b2-34363bc9ddd6/", false, "_|id=clients.client.cd7ec34a-5d4a-11e5-b0b2-34363bc9ddd6|_|pth=/AQjN/BVf/78w/QjNBVfWKH78w/clients|(end)" },
		{ "/favicon.ico", false, "_|id=favicon.ico|_|pth=|(end)" },
		{ "path/1,path/2,path/3/ID/:search", false, "_|cmd=:search|id=ID|_|pth=path/1|_|pth=path/2|_|pth=path/3|(end)" },
		{ ",path/1,path/2,path/3/ID/:search", false, "_|cmd=:search|id=ID|_|pth=|_|pth=path/1|_|pth=path/2|_|pth=path/3|(end)" },
		{ "path/1,,path/2,path/3/ID/:search", false, "_|cmd=:search|id=ID|_|pth=path/1|_|pth=|_|pth=path/2|_|pth=path/3|(end)" },
		{ "path/1,path/2,,path/3/ID/:search", false, "_|cmd=:search|id=ID|_|pth=path/1|_|pth=path/2|_|pth=|_|pth=path/3|(end)" },
		{ "path/1,path/2,path/3,/ID/:search", false, "_|cmd=:search|id=ID|_|pth=path/1|_|pth=path/2|_|pth=path/3|_|pth=|(end)" },
		{ "1", false, "_|id=1|_|pth=|(end)" },
		{ "1/", false, "_|id=1|_|pth=|(end)" },
		{ "/1", false, "_|id=1|_|pth=|(end)" },
		{ "/1/", false, "_|id=1|_|pth=|(end)" },
		{ "/1,/2", false, "_|id=2|_|pth=/1|_|pth=|(end)" },
		{ "1,2", true, "_|_|pth=1|_|pth=2|(end)" },
		{ "1/,2/", true, "_|_|pth=1/|_|pth=2/|(end)" },
		{ "/1,/2", true, "_|_|pth=/1|_|pth=/2|(end)" },
		{ "/twitter/tweet/:metadata/_schema", true, "_|cmd=:metadata|pmt=_schema|_|pth=/twitter/tweet|(end)" },
		{ "/twitter/tweet/:metadata/_schema|version", true, "_|slc=version|cmd=:metadata|pmt=_schema|_|pth=/twitter/tweet|(end)" },
		{ "/twitter/tweet/1/|user.name", false, "_|slc=user.name|id=1|_|pth=/twitter/tweet|(end)" },
		{ "/twitter/tweet/1/|{user.name}", false, "_|slc={user.name}|id=1|_|pth=/twitter/tweet|(end)" },
	};

	int count = 0;
	for (const auto& url : urls) {
		std::string result = run_url_path(url.path, url.clear_id);
		if (result != url.expected) {
			L_ERR("Error: the value obtained from the url path: {{ \"{}\", {} }}\n  should be:\n    {}\n  but it is:\n    {}\n", url.path, url.clear_id ? "true" : "false", url.expected, result);
			++count;
		}
	}

	RETURN(count);
}
