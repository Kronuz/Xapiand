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

// The Xapian data guard (xapian_guard::find): the Xapian backend's file names are found in the registry
// shard and in an index's shard directories, strata's own files and a clean directory are not.

#include "test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "xapian_data_guard.h"

namespace {

std::string make_root() {
	std::string tmpl = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : ".") + "/xguard.XXXXXX";
	CHECK(::mkdtemp(tmpl.data()) != nullptr);
	return tmpl;
}

void mkdirs(const std::string& path) {
	for (size_t pos = 1; (pos = path.find('/', pos)) != std::string::npos; ++pos) {
		::mkdir(path.substr(0, pos).c_str(), 0755);
	}
	::mkdir(path.c_str(), 0755);
}

void touch(const std::string& path) {
	mkdirs(path.substr(0, path.rfind('/')));
	if (auto* f = std::fopen(path.c_str(), "w")) {
		std::fclose(f);
	}
}

void remove_tree(const std::string& root) {
	std::string cmd = "rm -rf '" + root + "'";
	(void)!std::system(cmd.c_str());
}

void test_names() {
	for (auto name : {"iamglass", "flintlock", "postlist.glass", "postlist.baseA", "record.glass", "termlist.glass"}) {
		CHECK(xapian_guard::is_xapian_file(name));
	}
	for (auto name : {"node", "glass", ".glass", "postlist", "iamglass.bak", "strata", "wal.0", "MANIFEST"}) {
		CHECK_FALSE(xapian_guard::is_xapian_file(name));
	}
}

void test_clean_directory() {
	auto root = make_root();
	CHECK_EQ(xapian_guard::find(root), "");
	touch(root + "/.xapiand/node");
	touch(root + "/.xapiand/indexes/.__1/strata/MANIFEST");
	touch(root + "/bank/.__1/strata/postlist.glass");  // strata's own directory is never entered
	touch(root + "/bank/.__1/strata/segment-1");
	CHECK_EQ(xapian_guard::find(root), "");
	CHECK_EQ(xapian_guard::find(root + "/missing"), "");
	remove_tree(root);
}

void test_registry_shard() {
	for (auto name : {"iamglass", "flintlock", "postlist.glass", "record.glass"}) {
		auto root = make_root();
		touch(root + "/.xapiand/indexes/.__1/" + name);
		CHECK_EQ(xapian_guard::find(root), root + "/.xapiand/indexes/.__1/" + name);
		remove_tree(root);
	}
}

void test_index_shards() {
	auto root = make_root();
	touch(root + "/.xapiand/indexes/.__1/strata/MANIFEST");
	touch(root + "/a/b/c/.__3/iamglass");
	touch(root + "/a/b/c/.__3/postlist.glass");
	auto found = xapian_guard::find(root);
	CHECK(found == root + "/a/b/c/.__3/iamglass" || found == root + "/a/b/c/.__3/postlist.glass");
	remove_tree(root);

	root = make_root();
	touch(root + "/solo/flintlock");  // an unsharded index
	CHECK_EQ(xapian_guard::find(root), root + "/solo/flintlock");
	remove_tree(root);
}

void test_registry_first() {
	auto root = make_root();
	touch(root + "/a/.__1/iamglass");
	touch(root + "/.xapiand/indexes/.__1/iamglass");
	CHECK_EQ(xapian_guard::find(root), root + "/.xapiand/indexes/.__1/iamglass");
	remove_tree(root);
}

void test_links_not_followed() {
	auto root = make_root();
	auto other = make_root();
	touch(other + "/iamglass");
	::symlink(other.c_str(), (root + "/link").c_str());
	CHECK_EQ(xapian_guard::find(root), "");
	remove_tree(root);
	remove_tree(other);
}

}  // namespace

TEST_MAIN(test_names, test_clean_directory, test_registry_shard, test_index_shards, test_registry_first, test_links_not_followed)
