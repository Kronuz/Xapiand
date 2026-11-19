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


// Coverage tests for src/endpoint.cc: an Endpoint resolves a URI to the path, node and query it names, a UUID in a
// path is normalized to the configured representation (and partitioned into directories when asked), and Endpoint
// and Endpoints compare, hash, sort and print by their path and node.

#include "test_coverage_core.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "endpoint.h"
#include "node.h"
#include "opts.h"
#include "serialize.h"
#include "xapiand_exception.h"

namespace {

const std::string LOCAL = "alpha";   // the local node's lower name
const std::string REMOTE = "beta";   // an active remote node
const std::string IDLE = "gamma";    // a node known but never touched

void set_up() {
	Node local;
	local.name("Alpha");
	local.host("127.0.0.1");
	local.http_port = 8880;
	Node::set_local_node(local);

	Node remote;
	remote.name("Beta");
	remote.host("127.0.0.2");
	remote.http_port = 8881;
	Node::touch_node(remote, true, true);

	Node idle;
	idle.name("Gamma");
	idle.host("127.0.0.3");
	idle.http_port = 8882;
	Node::touch_node(idle, false, false);

	Endpoint::cwd = "/data/";
	opts.uuid_partition = false;
	opts.uuid_repr = static_cast<uint32_t>(UUIDRepr::vanilla);
}

const std::string UUID_A = "00000000-0000-1000-8000-010000000000";

}  // namespace

static void test_file_uri() {
	set_up();
	Endpoint e("idx");
	SEE(e.path, std::string("idx"));
	SEE(e.node_name, LOCAL);
	SEE(e.query, std::string());
	SEE(e.to_string(), "xapiand://" + LOCAL + "/idx");
	CHECK_TRUE(e.is_local());
	CHECK_FALSE(e.empty());

	// The host of a file URI is the first directory of its path, and a query is dropped.
	SEE(Endpoint("file://dir/idx").path, std::string("dir/idx"));
	SEE(Endpoint("file://dir/idx").node_name, LOCAL);
	SEE(Endpoint("idx?x=1").path, std::string("idx"));
	SEE(Endpoint("idx?x=1").query, std::string());
	SEE(Endpoint("a/b/c").path, std::string("a/b/c"));
	SEE(Endpoint("/data/a//b/").path, std::string("a/b"));
}

static void test_node_uri() {
	set_up();
	// A node named in the URI is the endpoint's node.
	Endpoint e("xapiand://beta/idx");
	SEE(e.path, std::string("idx"));
	SEE(e.node_name, REMOTE);
	SEE(e.to_string(), "xapiand://" + REMOTE + "/idx");
	CHECK_FALSE(e.is_local());
	CHECK_TRUE(e.is_active());
	CHECK_TRUE(e.node() != nullptr);
	SEE(e.node()->name(), std::string("Beta"));
	SEE(Endpoint("xapiand://BETA/a/b").node_name, REMOTE);  // node names compare in lower case

	// A query follows the path.
	Endpoint q("xapiand://beta/idx?x=1&y=2");
	SEE(q.path, std::string("idx"));
	SEE(q.query, std::string("x=1&y=2"));
	SEE(q.to_string(), "xapiand://" + REMOTE + "/idx?x=1&y=2");
	// An empty query is not one: the '?' stays in the name.
	SEE(Endpoint("xapiand://beta/idx?").path, std::string("idx?"));
	SEE(Endpoint("xapiand://beta/idx?").query, std::string());

	// No node in the URI is the local node.
	SEE(Endpoint("xapiand:///idx").node_name, LOCAL);
	CHECK_TRUE(Endpoint("xapiand:///idx").is_local());

	// A node that is not known leaves the endpoint without one; so does one given with a port.
	Endpoint unknown("xapiand://nowhere/idx");
	SEE(unknown.node_name, std::string());
	SEE(unknown.path, std::string("idx"));
	CHECK_FALSE(unknown.is_local());
	CHECK_FALSE(unknown.is_active());
	CHECK_TRUE(unknown.node() == nullptr);
	// Pinned (endpoint-host-port): "host:port" in the URI drops the host.
	SEE(Endpoint("xapiand://beta:8881/idx").node_name, std::string());

	// A node known but never touched is not active.
	Endpoint idle("xapiand://gamma/idx");
	SEE(idle.node_name, IDLE);
	CHECK_FALSE(idle.is_active());

	// The node given to the constructor is the endpoint's node, whatever the URI names.
	auto beta = Node::get_node("beta");
	REQUIRE(beta != nullptr);
	SEE(Endpoint("idx", beta).node_name, REMOTE);
	SEE(Endpoint("xapiand://gamma/idx", beta).node_name, REMOTE);
}

static void test_copy_to_node() {
	set_up();
	auto beta = Node::get_node("beta");
	REQUIRE(beta != nullptr);
	const Endpoint original("xapiand://alpha/idx?k=v");
	SEE(original.query, std::string("k=v"));

	Endpoint copy(original, beta);
	SEE(copy.path, std::string("idx"));
	SEE(copy.query, std::string("k=v"));
	SEE(copy.node_name, REMOTE);
	SEE(original.node_name, LOCAL);  // the original is untouched

	Endpoint source("xapiand://alpha/other?z=1");
	Endpoint moved(std::move(source), beta);
	SEE(moved.path, std::string("other"));
	SEE(moved.query, std::string("z=1"));
	SEE(moved.node_name, REMOTE);
}

static void test_empty_and_order() {
	set_up();
	Endpoint none;
	CHECK_TRUE(none.empty());
	SEE(none.to_string(), std::string());
	Endpoint only_query;
	only_query.query = "q";
	CHECK_FALSE(only_query.empty());
	SEE(only_query.to_string(), std::string());  // without a path there is nothing to print

	const Endpoint a("xapiand://alpha/a"), b("xapiand://alpha/b"), a_beta("xapiand://beta/a");
	CHECK_TRUE(a < b);
	CHECK_FALSE(b < a);
	CHECK_FALSE(a < a);
	CHECK_TRUE(a < a_beta);        // the same path: the node name decides
	CHECK_TRUE(a_beta < b);        // the path decides before the node
	CHECK_TRUE(a == Endpoint("xapiand://alpha/a?ignored=1"));  // a query is not part of the identity
	CHECK_TRUE(a != b);
	CHECK_TRUE(a != a_beta);
	SEE(std::hash<Endpoint>()(a), std::hash<std::string>()("a") ^ std::hash<std::string>()(LOCAL));
	SEE(a.hash(), std::hash<Endpoint>()(a));
	CHECK_TRUE(a.hash() == Endpoint("xapiand://alpha/a").hash());
	CHECK_TRUE(a.hash() != b.hash());
}

static void test_endpoints() {
	set_up();
	const Endpoint a("xapiand://alpha/a"), b("xapiand://alpha/b"), c("xapiand://beta/b");
	Endpoints list;
	CHECK_TRUE(list.empty());
	SEE(list.to_string(), std::string());
	SEE(list.hash(), size_t(0));
	list.add(b);
	list.add(a);
	list.add(c);
	list.add(a);  // a duplicate is not added
	SEE(list.size(), size_t(3));
	SEE(list.to_string(), a.to_string() + ";" + b.to_string() + ";" + c.to_string());
	CHECK_TRUE(list[0] == a);
	CHECK_TRUE(list.front() == a);
	CHECK_TRUE(list.back() == c);

	Endpoints same;
	same.add(c);
	same.add(b);
	same.add(a);
	CHECK_TRUE(list == same);
	CHECK_FALSE(list != same);
	SEE(list.hash(), same.hash());
	SEE(std::hash<Endpoints>()(list), list.hash());
	SEE(list.hash(), a.hash() ^ b.hash() ^ c.hash());

	Endpoints shorter(a);
	SEE(shorter.size(), size_t(1));
	CHECK_FALSE(list == shorter);
	CHECK_TRUE(list != shorter);
	Endpoints other;
	other.add(a);
	other.add(b);
	other.add(Endpoint("xapiand://beta/z"));
	CHECK_FALSE(list == other);
	CHECK_TRUE(list != other);
}

namespace {

// The path an index named by `uuid` gets under `repr`, with the UUIDs normalized and, when `partition`, partitioned.
std::string path_of(const std::string& uuid, UUIDRepr repr, bool partition) {
	opts.uuid_repr = static_cast<uint32_t>(repr);
	opts.uuid_partition = partition;
	return Endpoint(uuid).path;
}

std::string normalized(const std::string& uuid, UUIDRepr repr) {
	return Unserialize::uuid(Serialize::uuid(uuid), repr);
}

}  // namespace

static void test_uuid_normalized() {
	set_up();
	const std::string upper = "A2D6E3B4-0000-1000-8000-0100000000AB";
	const std::string lower = "a2d6e3b4-0000-1000-8000-0100000000ab";
	SEE(path_of(upper, UUIDRepr::vanilla, false), lower);
	SEE(path_of(lower, UUIDRepr::vanilla, false), lower);
#ifdef XAPIAND_UUID_GUID
	SEE(path_of(upper, UUIDRepr::guid, false), "{" + lower + "}");
#endif
#ifdef XAPIAND_UUID_URN
	SEE(path_of(upper, UUIDRepr::urn, false), "urn:uuid:" + lower);
#endif
#ifdef XAPIAND_UUID_ENCODED
	{
		const auto enc = path_of(upper, UUIDRepr::encoded, false);
		SEE(enc, normalized(upper, UUIDRepr::encoded));
		CHECK_TRUE(!enc.empty() && (enc[0] == '~' || enc == lower));
	}
#endif
	// Only the segments that are UUIDs change: a UUID between two names, and one next to a dot.
	opts.uuid_repr = static_cast<uint32_t>(UUIDRepr::vanilla);
	opts.uuid_partition = false;
	SEE(Endpoint("idx/" + upper + "/tail").path, "idx/" + lower + "/tail");
	SEE(Endpoint("idx." + upper).path, "idx." + lower);
	SEE(Endpoint("not-a-uuid/0000").path, std::string("not-a-uuid/0000"));
	// Several UUIDs in a segment, separated by the list separator, all normalize.
	SEE(Endpoint(upper + ";" + upper).path, lower + ";" + lower);
}

static void test_uuid_partitioned() {
	set_up();
	const std::string n = UUID_A;
	// vanilla: characters 14-17, then 9-12, then the whole UUID.
	SEE(path_of(UUID_A, UUIDRepr::vanilla, true), n.substr(14, 4) + "/" + n.substr(9, 4) + "/" + n);
	SEE(path_of(UUID_A, UUIDRepr::vanilla, true), std::string("1000/0000/") + UUID_A);
	// Inside a longer path, and a name that is not a UUID stays as it is.
	opts.uuid_repr = static_cast<uint32_t>(UUIDRepr::vanilla);
	opts.uuid_partition = true;
	SEE(Endpoint("idx/" + UUID_A).path, "idx/1000/0000/" + UUID_A);
	SEE(Endpoint("plain/name").path, std::string("plain/name"));
#ifdef XAPIAND_UUID_GUID
	{
		const auto g = normalized(UUID_A, UUIDRepr::guid);
		SEE(g, "{" + UUID_A + "}");
		SEE(path_of(UUID_A, UUIDRepr::guid, true), g.substr(15, 4) + "/" + g.substr(10, 4) + "/" + g);
	}
#endif
#ifdef XAPIAND_UUID_URN
	{
		const auto u = normalized(UUID_A, UUIDRepr::urn);
		SEE(u, "urn:uuid:" + UUID_A);
		SEE(path_of(UUID_A, UUIDRepr::urn, true), u.substr(23, 4) + "/" + u.substr(18, 4) + "/" + u);
	}
#endif
#ifdef XAPIAND_UUID_ENCODED
	// The encoded form of a condensed UUID is split 1/2/2/2/rest; any other form is partitioned as the vanilla one is.
	for (const char* text : { "00000000-0000-1000-8000-010000000000", "a2d6e3b4-0000-1000-8000-0100000000ab",
	                          "11111111-2222-1333-8444-555555555555", "a2d6e3b4-c5f6-4a7b-8c9d-0e1f2a3b4c5d",
	                          "e3e70682-c209-4cac-629f-ccf211ecf0f0", "6ba7b810-9dad-11d1-80b4-00c04fd430c8" }) {
		const std::string uuid = text;
		const auto serialized = Serialize::uuid(uuid);
		const auto norm = normalized(uuid, UUIDRepr::encoded);
		const bool condensed = serialized.front() != 1 && ((serialized.back() & 1) != 0 || (serialized.size() > 5 && (*(serialized.rbegin() + 5) & 2) != 0));
		const auto got = path_of(uuid, UUIDRepr::encoded, true);
		if (condensed) {
			REQUIRE(norm.size() >= 8);
			const auto want = norm.substr(0, 1) + "/" + norm.substr(1, 2) + "/" + norm.substr(3, 2) + "/" + norm.substr(5, 2) + "/" + norm.substr(7);
			SEE(got, want);
		} else {
			REQUIRE(norm.size() >= 18);
			SEE(got, norm.substr(14, 4) + "/" + norm.substr(9, 4) + "/" + norm);
		}
	}
#endif
}

TEST_MAIN(test_file_uri, test_node_uri, test_copy_to_node, test_empty_and_order, test_endpoints, test_uuid_normalized,
          test_uuid_partitioned)
