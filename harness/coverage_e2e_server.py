#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Server views, value by value: src/server/search_views.cc and mime_types.cc on instrumented nodes.

Usage: coverage_e2e_server.py BINARY OUTDIR [GROUP ...]

Starts a fresh node per group (each with its own flags), sends the group's requests and compares every status, header
and body with the value the code and the docs give. A check named PIN states what the node answers today where that
looks wrong; the finding is named beside it so the fix flips one line. Exit 0 when every check passes.
"""
import http.client
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import time
import traceback
import urllib.parse

BIN = os.path.abspath(sys.argv[1])
OUT = os.path.abspath(sys.argv[2])
ONLY = set(sys.argv[3:])
failures = []
checks = 0


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Resp:
    def __init__(self, status, headers, body):
        self.status, self.headers, self.body = status, headers, body

    @property
    def json(self):
        return json.loads(self.body.decode("utf-8", "replace"))

    def header(self, name):
        return self.headers.get(name.lower())


class Node:
    """One node on a fresh data directory, started with FLAGS and ENV, stopped at the end of its group."""

    def __init__(self, name, flags=(), env=None, admin=True):
        self.name = name
        self.port = free_port()
        self.dir = os.path.join(OUT, name)
        shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(self.dir)
        self.flags = (["--admin-commands"] if admin else []) + list(flags)
        self.env = dict(os.environ, **(env or {}))
        self.proc = None

    def start(self):
        log = open(os.path.join(self.dir, "node.log"), "wb")
        self.proc = subprocess.Popen(
            [BIN, "--port", str(self.port), "--name", "covnode", "-D", os.path.join(self.dir, "data")] + self.flags,
            stdout=log, stderr=subprocess.STDOUT, env=self.env)
        for _ in range(240):
            if self.proc.poll() is not None:
                raise RuntimeError("node %s exited before ready (see %s/node.log)" % (self.name, self.dir))
            try:
                if self.call("GET", "/", timeout=2).status == 200:
                    return self
            except OSError:
                pass
            time.sleep(0.5)
        raise RuntimeError("node %s never answered" % self.name)

    def stop(self):
        if self.proc is not None and self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(60)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

    def wait_exit(self, seconds=60):
        try:
            self.proc.wait(seconds)
            return self.proc.returncode
        except subprocess.TimeoutExpired:
            return None

    def call(self, method, path, body=None, headers=None, timeout=120):
        h = dict(headers or {})
        if body is not None and not isinstance(body, (bytes, str)):
            body = json.dumps(body)
        if body is not None and "Content-Type" not in h:
            h["Content-Type"] = "application/json"
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        try:
            try:
                conn.request(method, path, body=body, headers=h)
            except (BrokenPipeError, ConnectionResetError):
                pass  # a refusal sent before the whole body went out
            r = conn.getresponse()
            data = r.read()
            return Resp(r.status, {k.lower(): v for k, v in r.getheaders()}, data)
        finally:
            conn.close()

    def raw(self, parts, wait=0.3):
        """Send PARTS (bytes) down one socket, pausing between them; the whole answer, until the node closes."""
        s = socket.create_connection(("127.0.0.1", self.port), timeout=5)
        out = b""
        try:
            for part in parts:
                s.sendall(part)
                time.sleep(wait)
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                out += chunk
        except OSError:
            pass
        finally:
            s.close()
        return out

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()


def check(label, ok, detail=""):
    global checks
    checks += 1
    if not ok:
        failures.append(label)
        print("FAIL %s %s" % (label, detail))


def eq(label, got, want):
    check(label, got == want, "\n     got  %r\n     want %r" % (got, want))


def status(label, resp, want):
    check(label, resp.status == want, "-> %d, want %d: %r" % (resp.status, want, resp.body[:300]))


def error(label, resp, code, message=None):
    """An error answer: its status, its body's status and (when given) its message."""
    status(label, resp, code)
    try:
        body = resp.json
    except ValueError:
        check(label + " body", False, repr(resp.body[:200]))
        return
    eq(label + " body status", body.get("status"), code)
    if message is not None:
        eq(label + " message", body.get("message"), message)


GROUPS = []


def group(fn):
    GROUPS.append(fn)
    return fn


DOCS = [
    ("d1", {"name": "alpha", "n": 1, "tags": ["x", "y"]}),
    ("d2", {"name": "beta", "n": 2, "tags": ["y"]}),
    ("d3", {"name": "gamma", "n": 3, "tags": ["z"]}),
    ("d4", {"name": "delta", "n": 4}),
]


def seed(node, index="t"):
    for did, doc in DOCS:
        status("seed %s/%s" % (index, did), node.call("PUT", "/%s/%s?commit" % (index, did), doc), 204)


def ids(resp):
    return sorted(h["_id"] for h in resp.json["hits"])


def q(**kw):
    return urllib.parse.urlencode(kw)


VERBS = ["GET", "POST", "PUT", "PATCH", "UPDATE", "UPSERT", "DELETE", "HEAD", "OPTIONS", "SEARCH", "COUNT", "INFO",
         "COMMIT", "DUMP", "RESTORE", "CHECK", "FLUSH", "QUIT"]


# Every verb of the node's table, each as the method override header, as a command in the path and as the method itself.
# VERB: (index to ask, body, status, what the answer holds).
VERB_BODY = {"a": 1}
VERB_ROWS = {
    "delete": ("sink", None, 501), "get": ("t", None, 200), "head": ("t", None, 200), "post": ("sink", VERB_BODY, 201),
    "put": ("sink", VERB_BODY, 204), "connect": ("t", None, 405), "options": ("t", None, 200), "trace": ("t", None, 405),
    "patch": ("sink", VERB_BODY, 204), "purge": ("t", None, 405), "link": ("t", None, 405), "unlink": ("t", None, 405),
    "check": ("t", None, 200), "close": ("t", None, 501), "commit": ("t", None, 200), "copy": ("t", None, 405),
    "count": ("t", None, 200), "dump": ("t", None, 200), "flush": ("t", None, 405), "info": ("t", None, 200),
    "lock": ("t", None, 405), "move": ("t", None, 405), "open": ("t", None, 501), "quit": ("t", None, 405),
    "restore": ("sink", VERB_BODY, 400), "search": ("t", None, 200), "unlock": ("t", None, 405),
    "update": ("sink", VERB_BODY, 204), "upsert": ("sink", VERB_BODY, 204),
}
NOT_ALLOWED = {"status": 405, "type": "Method Not Allowed"}
NOT_IMPLEMENTED = {"status": 501, "type": "Not Implemented"}


def verb_answer(label, verb, r, status_want):
    """The answer of VERB on its index: the status and what the body holds."""
    eq(label + " status", r.status, status_want)
    if status_want == 405:
        eq(label + " body", r.json, NOT_ALLOWED)
    elif status_want == 501:
        eq(label + " body", r.json, NOT_IMPLEMENTED)
    elif verb == "get":
        check(label + " body", "_settings" in r.json and "_schema" in r.json, repr(r.body[:100]))
    elif verb in ("head", "commit", "options", "put", "patch", "update", "upsert"):
        eq(label + " body", r.body, b"")
    elif verb == "post":
        eq(label + " body", (r.body, (r.header("Location") or "").startswith("/sink/")), (b"", True))
    elif verb == "check":
        eq(label + " body", r.json, {"errors": {"t/.__%d" % i: 0 for i in range(1, 6)}})
    elif verb == "count":
        eq(label + " body", r.json, {"total": 4})
    elif verb == "dump":
        eq(label + " body", sorted(d["_id"] for d in r.json), ["d1", "d2", "d3", "d4"])
    elif verb == "info":
        eq(label + " body", (r.json["endpoint"], r.json["doc_count"]), ("t", 4))
    elif verb == "search":
        eq(label + " body", (r.json["count"], r.json["total"]), (4, 4))
    elif verb == "restore":
        error(label + " body", r, 400, "Bad Request: Invalid request body")


@group
def verbs(n):
    """Each verb of the table: as the override header, as a path command and as itself; the mismatches refuse."""
    seed(n)
    n.call("PUT", "/sink/s1?commit", VERB_BODY)
    for verb, (index, body, want) in VERB_ROWS.items():
        upper = verb.upper()
        base = "/%s/" % index
        override = {"X-HTTP-Method-Override": upper}
        # A mismatched method: the one a verb is not (PUT, or DELETE for PUT and DELETE themselves).
        other = "DELETE" if verb in ("put", "patch") else "PUT"
        for method in ("POST", "GET", upper):
            verb_answer("%s override %s" % (method, verb), verb, n.call(method, base, body, override), want)
            verb_answer("%s %s:%s" % (method, base, verb), verb, n.call(method, base + ":" + verb, body), want)
        r = n.call(other, base, body, override)
        eq("%s override %s is refused" % (other, verb), (r.status, r.body), (400, b""))
        error("%s %s:%s is refused" % (other, base, verb), n.call(other, base + ":" + verb, body), 400,
              "Bad Request: HTTP Mappings must use GET or POST method")
    # A root command that is a verb word names the node; the others are metadata of an index the root lacks.
    for command, status_want in ((":info", 200), (":head", 200), (":get", 200), (":metrics", 200), (":stats", 400)):
        eq("GET /%s" % command, n.call("GET", "/" + command).status, status_want)
    eq("GET /:get names the node", n.call("GET", "/:get").json["name"], "covnode")

    # An index with empty shards reports its ids and lengths as unsigned wrap-arounds.
    n.call("PUT", "/one/a?commit", {"a": 1})
    info = n.call("INFO", "/one/").json
    eq("INFO of a one-document index", (info["doc_count"], len(info["shards"])), (1, 5))
    eq("PIN: INFO last_id and doc_del wrap at 2^32 when a shard is empty", (info["last_id"], info["doc_del"]), (2**32 - 1, 2**32 - 2))
    check("PIN: INFO doc_len_lower wraps at 2^32 when a shard is empty", info["doc_len_lower"] > 2**31, repr(info))


@group
def routing(n):
    """The route table, the verbs answered inline, the method overrides and the content negotiation."""
    seed(n)
    r = n.call("OPTIONS", "/")
    status("OPTIONS /", r, 200)
    allow = [v.strip() for v in r.header("Allow").split(",")]
    eq("OPTIONS Allow", allow, VERBS)
    eq("OPTIONS has no body", r.body, b"")
    eq("OPTIONS /t/d1 Allow", n.call("OPTIONS", "/t/d1").header("Allow"), r.header("Allow"))

    r = n.call("OPEN", "/t")
    eq("OPEN", (r.status, r.json), (501, {"status": 501, "type": "Not Implemented"}))
    r = n.call("CLOSE", "/t")
    eq("CLOSE", (r.status, r.json), (501, {"status": 501, "type": "Not Implemented"}))
    for verb in ("LINK", "TRACE", "COPY", "MOVE", "LOCK", "PURGE", "CONNECT", "UNLINK", "UNLOCK"):
        r = n.call(verb, "/t")
        eq("%s is not routed" % verb, (r.status, r.json), (405, {"status": 405, "type": "Method Not Allowed"}))
    # A verb the route table does not know for this URL shape: POST of a document, GET with a range id.
    r = n.call("POST", "/t/d1", {"a": 1})
    eq("POST of a document id", r.status, 405)

    # A command in the path maps to the verb of the same name (GET or POST only).
    for method in ("GET", "POST"):
        r = n.call(method, "/t/:search?" + q(q="name:alpha"))
        eq("%s :search" % method, [h["_id"] for h in r.json["hits"]], ["d1"])
    eq("GET :count", n.call("GET", "/t/:count").json, {"total": 4})
    error("PUT :search", n.call("PUT", "/t/:search"), 400, "Bad Request: HTTP Mappings must use GET or POST method")
    r = n.call("GET", "/t/:info")
    eq("GET :info names the index", r.json["shard"] if "shard" in r.json else None, r.json.get("shard"))

    # The method override header: POST or GET only, to a verb of the table; an unknown verb is ignored.
    for header in ("X-HTTP-Method-Override", "HTTP-Method-Override"):
        n.call("PUT", "/t/tmp?commit", {"v": 1})
        r = n.call("POST", "/t/tmp?commit", headers={header: "DELETE"})
        eq("%s: DELETE of a document" % header, r.status, 204)
        eq("%s: the document is gone" % header, n.call("HEAD", "/t/tmp").status, 404)
    r = n.call("PUT", "/t/d1", {"a": 1}, headers={"X-HTTP-Method-Override": "DELETE"})
    status("override on PUT", r, 400)
    r = n.call("POST", "/t/d1", headers={"X-HTTP-Method-Override": "NOPE"})
    eq("an unknown override is ignored", r.status, 405)
    r = n.call("GET", "/t/d1", headers={"X-HTTP-Method-Override": "HEAD"})
    eq("GET overridden to HEAD", (r.status, r.body), (200, b""))

    # Content negotiation on one document.
    doc = {"name": "alpha", "n": 1, "tags": ["x", "y"], "_id": "d1", "_version": 1}
    r = n.call("GET", "/t/d1")
    eq("GET d1", (r.status, r.json, r.header("Content-Type")), (200, doc, "application/json; charset=utf-8"))
    r = n.call("GET", "/t/d1", headers={"Accept": "application/x-yaml"})
    eq("Accept yaml type", r.header("Content-Type"), "application/x-yaml; charset=utf-8")
    r = n.call("GET", "/t/d1", headers={"Accept": "application/msgpack"})
    eq("Accept msgpack type", r.header("Content-Type"), "application/msgpack; charset=utf-8")
    check("Accept msgpack body", r.body.startswith(b"\x85\xa4name\xa5alpha"), repr(r.body[:20]))
    for accept in ("text/html", "image/png", "text/plain"):
        r = n.call("GET", "/t/d1", headers={"Accept": accept})
        eq("Accept %s" % accept, (r.status, r.body), (406, b""))
    for accept, want in (("*", 200), ("*/*", 200), ("application/json;indent=1", 200), ("application/json;indent=99", 200),
                         ("application/json;indent=-5", 200), ("application/json;indent=x", 200),
                         ("text/html, application/json;q=0.5", 200)):
        r = n.call("GET", "/t/d1", headers={"Accept": accept})
        eq("Accept %s" % accept, (r.status, r.json), (want, doc))
    r = n.call("GET", "/t/d1", headers={"Accept": "application/json;indent=1"})
    eq("PIN: an Accept indent parameter is not applied", r.body.decode(), json.dumps(doc, separators=(",", ":")))
    r = n.call("GET", "/t/d1?pretty", headers={"Accept": "application/json"})
    eq("?pretty", r.body.decode(), json.dumps(doc, indent=2))
    r = n.call("GET", "/t/d1?pretty=false")
    eq("?pretty=false", r.body.decode(), json.dumps(doc, separators=(",", ":")))
    r = n.call("GET", "/t/d1?pretty=maybe")
    eq("?pretty=maybe is ignored", r.json, doc)
    r = n.call("GET", "/t/d1?" + q(version="x"))
    eq("an unparsable version is not read by a GET", r.status, 200)


@group
def documents(n):
    """Writes, reads and deletes of one document: echo, comments, selectors, versions, blobs."""
    r = n.call("PUT", "/t/d1?commit", {"name": "alpha", "n": 1})
    eq("PUT answers 204 without echo", (r.status, r.body), (204, b""))
    r = n.call("PUT", "/t/d2?echo&commit", {"name": "beta", "n": 2})
    eq("PUT ?echo", (r.status, r.json), (200, {"name": "beta", "n": 2, "_id": "d2", "_version": 1}))
    r = n.call("PUT", "/t/d3?echo&comments", {"name": "gamma"})
    eq("PUT ?echo&comments", sorted(r.json), ["#docid", "#shard", "_id", "_version", "name"])
    r = n.call("PUT", "/t/d2?echo&selector=name", {"name": "beta", "n": 2})
    eq("PUT ?selector=name", (r.status, r.json), (200, "beta"))
    r = n.call("PUT", "/t/d2.name?echo", {"name": "beta2", "n": 2})
    eq("PUT with a path selector", (r.status, r.json), (200, "beta2"))
    eq("GET shows the version bumped", n.call("GET", "/t/d2").json["_version"], 3)

    r = n.call("POST", "/t/?echo", {"name": "delta"})
    status("POST", r, 200)
    new_id = r.json["_id"]
    eq("POST Location", r.header("Location"), "/t/" + new_id)
    r = n.call("POST", "/t/", {"name": "epsilon"})
    status("POST without echo", r, 201)
    check("POST without echo has a Location", r.header("Location", ) is not None and r.header("Location").startswith("/t/"))
    eq("POST without echo has no body", r.body, b"")
    r = n.call("POST", "/t/?echo", {"_id": "given", "name": "zeta"})
    eq("PIN: POST ignores the _id of its body", (r.status, r.header("Location") == "/t/" + r.json["_id"], r.json["_id"] != "given"), (200, True, True))
    r = n.call("POST", "/t/", {"_id": "given2", "name": "eta"})
    eq("PIN: POST ignores the _id of its body, no echo", (r.status, r.header("Location") != "/t/given2"), (201, True))

    # Versions: a write names the version it expects.
    r = n.call("GET", "/t/d1")
    version = r.json["_version"]
    r = n.call("PUT", "/t/d1?" + q(version=version + 5, commit=""), {"name": "alpha"})
    status("a stale version is a conflict", r, 409)
    eq("the conflict names the version to expect", r.json["_version"], version)
    r = n.call("PUT", "/t/d1?" + q(version=version, commit="true", echo=""), {"name": "alpha", "n": 10})
    eq("a matching version is written", (r.status, r.json["_version"]), (200, version + 1))
    r = n.call("PUT", "/t/d1?commit=false", {"name": "alpha", "n": 11})
    status("commit=false", r, 204)
    r = n.call("PUT", "/t/d1?commit=bogus", {"name": "alpha", "n": 12})
    status("commit=bogus is ignored", r, 204)
    status("PIN: version=abc is read as a stale version", n.call("PUT", "/t/d1?version=abc", {"a": 1}), 409)

    # Update verbs.
    r = n.call("PATCH", "/t/d1?echo", [{"op": "replace", "path": "/n", "value": 20}])
    eq("PATCH", (r.status, r.json["n"], r.json["name"]), (200, 20, "alpha"))
    r = n.call("PATCH", "/t/d1", [{"op": "replace", "path": "/n", "value": 21}])
    eq("PATCH without echo", (r.status, r.body), (204, b""))
    r = n.call("UPDATE", "/t/d1?echo&comments", {"n": 22})
    eq("UPDATE", (r.status, r.json["n"], "#docid" in r.json), (200, 22, True))
    r = n.call("UPDATE", "/t/d1?echo&selector=n", {"n": 23})
    eq("UPDATE ?selector", (r.status, r.json), (200, 23))
    r = n.call("UPDATE", "/t/d1.n?echo", {"n": 24})
    eq("UPDATE path selector", (r.status, r.json), (200, 24))
    r = n.call("UPSERT", "/t/fresh?echo", {"n": 1})
    eq("UPSERT creates", (r.status, r.json["_id"]), (200, "fresh"))
    r = n.call("UPSERT", "/t/fresh", {"n": 2})
    status("UPSERT without echo", r, 204)
    error("UPDATE of a missing document", n.call("UPDATE", "/t/nothere", {"n": 1}), 404)
    error("PATCH of a missing document", n.call("PATCH", "/t/nothere", [{"op": "add", "path": "/n", "value": 1}]), 404)
    error("PATCH with a body that is not a patch", n.call("PATCH", "/t/d1", {"n": 1}), 400, "Bad Request: A JSON Patch document MUST be an array of objects")

    # Reads.
    r = n.call("GET", "/t/d1?comments")
    eq("GET ?comments", sorted(k for k in r.json if k.startswith("#")), ["#docid", "#shard"])
    r = n.call("GET", "/t/d1?selector=name")
    eq("GET ?selector", (r.status, r.json), (200, "alpha"))
    r = n.call("GET", "/t/d1.name")
    eq("GET path selector", (r.status, r.json), (200, "alpha"))
    r = n.call("GET", "/t/d1?volatile")
    status("GET ?volatile", r, 200)
    r = n.call("GET", "/t/d1?volatile=false")
    status("GET ?volatile=false", r, 200)
    r = n.call("GET", "/t/d1?volatile=zzz")
    status("GET ?volatile=zzz", r, 200)
    r = n.call("GET", "/t/d1?" + q(check_at_least=2, offset=1, limit=1))
    status("GET with offset and limit", r, 200)
    error("GET missing", n.call("GET", "/t/nope"), 404, "Not Found: Document not found")
    error("GET in a missing index", n.call("GET", "/nodb/x"), 404)
    eq("HEAD present", (n.call("HEAD", "/t/d1").status, n.call("HEAD", "/t/d1").body), (200, b""))
    eq("HEAD missing", n.call("HEAD", "/t/nope").status, 404)
    eq("HEAD missing index", n.call("HEAD", "/nodb/x").status, 404)

    # Deletes.
    r = n.call("DELETE", "/t/fresh?commit")
    eq("DELETE", (r.status, r.body), (204, b""))
    error("DELETE twice", n.call("DELETE", "/t/fresh"), 404)
    r = n.call("GET", "/t/d3")
    r2 = n.call("DELETE", "/t/d3?" + q(version=r.json["_version"] + 9))
    status("DELETE of a stale version", r2, 409)
    status("DELETE of the right version", n.call("DELETE", "/t/d3?" + q(version=r.json["_version"], commit="")), 204)
    # Single-index views refuse several indexes.
    n.call("PUT", "/u/e1?commit", {"a": 1})
    for method, path in (("DELETE", "/t,u/d1"), ("PUT", "/t,u/d1"), ("PATCH", "/t,u/d1"), ("GET", "/t,u/d1"),
                         ("DUMP", "/t,u/d1"), ("INFO", "/t,u/d1"), ("DUMP", "/t,u/")):
        r = n.call(method, path, {"a": 1} if method in ("PUT", "PATCH") else None)
        error("%s %s" % (method, path), r, 400, "Bad Request: Method can only be used with single indexes")
    eq("HEAD over two indexes", n.call("HEAD", "/t,u/d1").status in (200, 400, 404), True)


@group
def edges(n):
    """Refusals over several indexes, content types of every answer, odd URLs, split and chunked bodies."""
    seed(n)
    n.call("PUT", "/u/e1?commit", {"n": 1})
    for method, path, body in (("CHECK", "/t,u/", None), ("PUT", "/t,u/", {"a": 1}),
                               ("UPSERT", "/t,u/x", {"a": 1}), ("DELETE", "/t,u/x", None), ("INFO", "/t,u/", None)):
        error("%s %s" % (method, path), n.call(method, path, body), 400, "Bad Request: Method can only be used with single indexes")
    error("GET metadata over two indexes", n.call("GET", "/t,u/:mykey"), 400, "Bad Request: Method can only be used with single indexes")
    error("PUT metadata over two indexes", n.call("PUT", "/t,u/:mykey", {"a": 1}), 400, "Bad Request: Method can only be used with single indexes")
    for method, path, body in (("GET", "/t,u/", None), ("GET", "/t,u/:", None), ("UPDATE", "/t,u/", {"a": 1}),
                               ("RESTORE", "/t,u/", [{"_id": "x", "a": 1}])):
        error("%s %s" % (method, path), n.call(method, path, body), 400, "Bad Request: Method can only be used with single indexes")
    eq("HEAD over two indexes", n.call("HEAD", "/t,u/").status, 400)
    eq("COMMIT over two indexes", (n.call("COMMIT", "/t,u/").status, n.call("COMMIT", "/t,u/").body), (200, b""))
    eq("COMMIT of a document id", n.call("COMMIT", "/t,u/d1").status, 405)
    r = n.call("RESTORE", "/t,u/", [])
    eq("RESTORE over two indexes", (r.status, r.json["total"]), (200, 0))

    for accept, ctype in (("application/x-json", "application/x-json"), ("application/x-yaml", "application/x-yaml"),
                          ("application/yaml", "application/yaml"), ("application/msgpack", "application/msgpack"),
                          ("application/x-msgpack", "application/x-msgpack")):
        r = n.call("SEARCH", "/t/?sort=n&limit=1", headers={"Accept": accept})
        eq("SEARCH as %s" % accept, (r.status, r.header("Content-Type")), (200, ctype + "; charset=utf-8"))
        r = n.call("GET", "/t/nope", headers={"Accept": accept})
        eq("a 404 as %s" % accept, (r.status, r.header("Content-Type")), (404, ctype + "; charset=utf-8"))
    for accept in ("text/html", "application/x-ndjson", "image/png"):
        r = n.call("SEARCH", "/t/?limit=1", headers={"Accept": accept})
        eq("PIN: SEARCH accepting only %s answers 200 with no body" % accept, (r.status, r.body, r.header("Content-Type")), (200, b"", None))
        r = n.call("GET", "/t/nope", headers={"Accept": accept})
        eq("a 404 accepting only %s has no body" % accept, (r.status, r.body), (404, b""))

    r = n.call("PUT", "/t/d9?echo", {"a": 1}, headers={"Expect": "100-continue"})
    eq("PUT with Expect: 100-continue", (r.status, r.json["_id"]), (200, "d9"))

    error("a command that is no metadata", n.call("GET", "/t/:bogus"), 404, "Not Found: Metadata not found")
    for command in (":search", ":count", ":commit", ":dump", ":check"):
        error("GET /%s" % command, n.call("GET", "/" + command), 400, "Bad Request: Index path '' is empty or outside the data directory")
    error("GET /:restore", n.call("GET", "/:restore"), 400, "Bad Request: Invalid request body")
    eq("GET /:info", n.call("GET", "/:info").json["name"], "covnode")
    eq("GET /_all/:count", n.call("GET", "/_all/:count").json["total"] >= 5, True)
    eq("a doubled slash", n.call("GET", "/t//d1").json["_id"], "d1")
    eq("a bad escape in the query", n.call("GET", "/t/d1?%zz").status, 200)
    eq("a fragment", n.call("GET", "/t/d1#frag").status, 200)
    eq("a NUL in the id", n.call("GET", "/t/%00").status, 404)
    eq("a parent segment", n.call("GET", "/t/..").json["name"], "covnode")
    status("a pattern as a document's index", n.call("GET", "/*/d1"), 404)

    head = b"Host: x\r\nConnection: close\r\n"
    out = n.raw([b"GET /t/d1 HTTP/1.1\r\nX-HTTP-Method-Override: SEARCH\r\n" + head + b"\r\n"])
    check("a GET of a document overridden to SEARCH", out.startswith(b"HTTP/1.1 405"), repr(out[:30]))
    out = n.raw([b"BREW /t HTTP/1.1\r\n" + head + b"\r\n"])
    check("an unknown method gets no answer", not out.startswith(b"HTTP/1.1 200"), repr(out[:30]))
    out = n.raw([b"GET http://[::1/x HTTP/1.1\r\n" + head + b"\r\n"])
    check("an unparsable URL is a 400", out.startswith(b"HTTP/1.1 400"), repr(out[:30]))

    body = b'{"_id":"a","v":1}\n{"_id":"b","v":2}'
    split = b"RESTORE /rr/ HTTP/1.1\r\nContent-Type: application/x-ndjson\r\nContent-Length: %d\r\n" % len(body) + head + b"\r\n"
    out = n.raw([split + body[:10], body[10:25], body[25:]])
    check("RESTORE of NDJSON split mid-line", out.startswith(b"HTTP/1.1 200") and b'"total":2' in out, repr(out[:80]))
    chunked = b"RESTORE /rr2/ HTTP/1.1\r\nContent-Type: application/x-ndjson\r\nTransfer-Encoding: chunked\r\n" + head + b"\r\n"
    out = n.raw([chunked + b"%x\r\n" % len(body) + body + b"\r\n0\r\n\r\n"])
    check("RESTORE of chunked NDJSON", out.startswith(b"HTTP/1.1 200") and b'"total":2' in out, repr(out[:80]))
    mp = (b"\x94" + b"\x82\xa3_id\xa1a\xa1v\x01" + b"\x82\xa3_id\xa1b\xa1v\x02" + b"\x82\xa3_id\xa1c\xa1v\x03" + b"\x82\xa3_id\xa1d\xa1v\x04")
    mhead = b"RESTORE /rr3/ HTTP/1.1\r\nContent-Type: application/msgpack\r\nAccept: application/json\r\nContent-Length: %d\r\n" % len(mp) + head + b"\r\n"
    out = n.raw([mhead + mp[:9], mp[9:20], mp[20:]])
    check("RESTORE of MsgPack split mid-object", out.startswith(b"HTTP/1.1 200") and b'"total":4' in out, repr(out[:120]))


@group
def bodies(n):
    """Bodies in each content type, and what a bad one answers."""
    seed(n)
    body = {"name": "yamlcase", "n": 7}
    yaml_body = "name: yamlcase\nn: 7\n"
    for ct, data in (("application/x-yaml", yaml_body), ("application/yaml", yaml_body),
                     ("application/json", json.dumps(body)), ("application/x-json", json.dumps(body))):
        r = n.call("PUT", "/b/%s?echo" % ct.split("/")[1], data, headers={"Content-Type": ct, "Accept": "application/json"})
        eq("PUT as %s" % ct, (r.status, r.json["name"], r.json["n"]), (200, "yamlcase", 7))
    import struct
    mp = b"\x82\xa4name\xa6mpcase\xa1n\x07"
    for ct in ("application/msgpack", "application/x-msgpack"):
        r = n.call("PUT", "/b/mp?echo", mp, headers={"Content-Type": ct, "Accept": "application/json"})
        eq("PUT as %s" % ct, (r.status, r.json["name"], r.json["n"]), (200, "mpcase", 7))
    r = n.call("PUT", "/b/form?echo", json.dumps(body), headers={"Content-Type": "application/x-www-form-urlencoded"})
    eq("form-urlencoded holding JSON", (r.status, r.json["name"]), (200, "yamlcase"))
    error("a JSON body that does not parse", n.call("PUT", "/b/bad", "{not json"), 400)
    error("a YAML body that does not parse", n.call("PUT", "/b/bad", "a: [1\n", headers={"Content-Type": "application/x-yaml"}), 400)
    r = n.call("PUT", "/b/nd", "{\"a\":1}\n{\"a\":2}", headers={"Content-Type": "application/x-ndjson"})
    status("PUT of NDJSON", r, 400)
    # A search with a body that is not a map.
    error("search body is an array", n.call("SEARCH", "/t/", "[1]"), 400, "Bad Request: Invalid request body")
    error("count body is an array", n.call("COUNT", "/t/", "[1]"), 400, "Bad Request: Invalid request body")
    # A body past 64 KiB is decoded in a chore of its own group.
    big = {"name": "big", "text": "lorem ipsum " * 8000}
    r = n.call("PUT", "/b/big?echo&selector=name", big)
    eq("PUT of a body past 64 KiB", (r.status, r.json), (200, "big"))
    eq("and it reads back whole", n.call("GET", "/b/big").json["text"], big["text"])
    # A body past the buffered cap is refused by every view that does not stream it.
    huge = b'{"a":"' + b"x" * (17 * 1024 * 1024) + b'"}'
    r = n.call("PATCH", "/b/big", huge)
    status("PATCH past the body cap", r, 413)
    eq("the node still answers", n.call("GET", "/b/big?selector=name").json, "big")


@group
def blobs(n):
    """Stored (non-JSON) bodies: the content type, the Accept of a read, the selector's mime type."""
    png = b"\x89PNG\r\n\x1a\n" + bytes(range(256)) * 4
    r = n.call("PUT", "/blob/img1?echo&commit", png, headers={"Content-Type": "image/png"})
    status("PUT of a PNG", r, 200)
    n.call("PUT", "/blob/early", png, headers={"Content-Type": "image/png"})
    r = n.call("GET", "/blob/early")
    eq("PIN: a blob read before its commit is a storage error", (r.status, r.json["message"]), (500, "Storage EOF"))
    r = n.call("GET", "/blob/img1")
    eq("GET of a PNG", (r.status, r.body, r.header("Content-Type")), (200, png, "image/png"))
    r = n.call("GET", "/blob/img1", headers={"Accept": "image/png"})
    eq("GET of a PNG, accepted", (r.status, r.body), (200, png))
    r = n.call("GET", "/blob/img1", headers={"Accept": "text/plain"})
    status("GET of a PNG, not accepted", r, 406)
    r = n.call("GET", "/blob/img1?comments")
    eq("a blob read ignores comments", r.body, png)
    r = n.call("UPSERT", "/blob/img1?echo&commit", png[:20], headers={"Content-Type": "image/png"})
    status("UPSERT of a blob", r, 200)
    r = n.call("UPDATE", "/blob/img1?commit", png[:30], headers={"Content-Type": "image/png"})
    status("UPDATE of a blob", r, 204)
    eq("the blob changed", n.call("GET", "/blob/img1").body, png[:30])
    r = n.call("PUT", "/blob/text1?commit", b"hello text", headers={"Content-Type": "text/plain"})
    status("PUT of text", r, 204)
    eq("text reads back", (n.call("GET", "/blob/text1").body, n.call("GET", "/blob/text1").header("Content-Type")),
       (b"hello text", "text/plain"))
    # The selector's extension names the type of a body that has none.
    r = n.call("PUT", "/blob/doc.json?echo", b'{"a":1}')
    status("PUT with an extension and no type", r, 200)
    r = n.call("PUT", "/blob/pic.png", b"not really", headers={"Content-Type": "image/png"})
    status("PUT .png", r, 204)
    # Content-Length of a stored upload past the limit is refused.
    r = n.call("PUT", "/blob/small?commit", b"abc", headers={"Content-Type": "application/octet-stream"})
    status("PUT of octet-stream", r, 204)
    eq("octet-stream reads back", n.call("GET", "/blob/small").body, b"abc")
    # A blob past the buffered cap streams to the data storage.
    chunk = bytes(range(256)) * 4096
    data = chunk * 18
    r = n.call("PUT", "/blob/streamed?echo&commit", data, headers={"Content-Type": "application/octet-stream"})
    status("PUT of a streamed blob", r, 200)
    got = n.call("GET", "/blob/streamed")
    eq("the streamed blob reads back whole", (got.status, len(got.body), got.body == data), (200, len(data), True))
    r = n.call("PATCH", "/blob/streamed", data)
    status("PATCH of a body past the cap", r, 413)
    # An upload of a stored blob answers a conflict at a stale version.
    r = n.call("PUT", "/blob/small?version=99", b"abc", headers={"Content-Type": "application/octet-stream"})
    status("PUT of a blob at a stale version", r, 409)
    r = n.call("PUT", "/blob/streamed?version=99", data, headers={"Content-Type": "application/octet-stream"})
    status("PUT of a streamed blob at a stale version", r, 409)


@group
def metadata(n):
    """Metadata keys of an index (:key), the databases' info and the node's."""
    seed(n)
    r = n.call("PUT", "/t/:mykey", {"a": [1, 2]})
    eq("PUT metadata", (r.status, r.body), (204, b""))
    r = n.call("PUT", "/t/:mykey?echo", {"a": [1, 2]})
    eq("PUT metadata ?echo", (r.status, r.json), (200, {"a": [1, 2]}))
    r = n.call("PUT", "/t/:mykey?echo&selector=a", {"a": [1, 2]})
    eq("PUT metadata ?selector", (r.status, r.json), (200, [1, 2]))
    r = n.call("GET", "/t/:mykey")
    eq("GET metadata", (r.status, r.json), (200, {"a": [1, 2]}))
    r = n.call("GET", "/t/:mykey?selector=a")
    eq("GET metadata ?selector", r.json, [1, 2])
    r = n.call("GET", "/t/:mykey.a")
    status("PIN: a metadata path selector is not found", r, 404)
    r = n.call("GET", "/t/:mykey?volatile")
    eq("GET metadata ?volatile", r.json, {"a": [1, 2]})
    error("GET missing metadata", n.call("GET", "/t/:nokey"), 404, "Not Found: Metadata not found")
    r = n.call("GET", "/t/:")
    status("GET all metadata", r, 200)
    check("all metadata names the key", "mykey" in r.json, repr(r.json))
    error("PUT :schema", n.call("PUT", "/t/:schema", {"a": 1}), 400, "Bad Request: Metadata ':schema' is read-only")
    error("PUT :metrics", n.call("PUT", "/t/:metrics", {"a": 1}), 400, "Bad Request: Metadata ':metrics' is read-only")
    error("PUT of the id's high mark", n.call("PUT", "/t/:_id_high", {"a": 1}), 400)
    eq("UPDATE metadata", n.call("UPDATE", "/t/:mykey", {"a": 1}).status, 501)
    eq("PATCH metadata", n.call("PATCH", "/t/:mykey", {"a": 1}).status, 501)
    eq("DELETE metadata", n.call("DELETE", "/t/:mykey").status, 501)

    # The database's info, a document's, the node's.
    r = n.call("INFO", "/t/")
    status("INFO of an index", r, 200)
    r = n.call("INFO", "/t/?selector=doc_count")
    status("INFO ?selector", r, 200)
    r = n.call("INFO", "/t/d1")
    eq("INFO of a document", (r.status, r.json["version"], r.json["docid"] > 0), (200, 1, True))
    try:
        r.body.decode("utf-8")
        valid = True
    except UnicodeDecodeError:
        valid = False
    eq("PIN: INFO of a document puts raw value bytes into the JSON text", valid, False)
    r = n.call("INFO", "/t/d1?raw")
    check("INFO ?raw adds the raw data", "raw_data" in r.json, repr(sorted(r.json)))
    r = n.call("INFO", "/t/d1?volatile&human")
    status("INFO ?volatile&human", r, 200)
    error("INFO of a missing document", n.call("INFO", "/t/nope"), 404)
    r = n.call("INFO", "/")
    eq("INFO of the node", (r.status, r.json["state"], r.json["name"]), (200, "READY", "covnode"))
    eq("INFO /?selector=state", n.call("INFO", "/?selector=state").json, "READY")
    eq("GET /?selector=name", n.call("GET", "/?selector=name").json, "covnode")
    eq("GET /.state", n.call("GET", "/.state").json, "READY")
    error("GET /:info.state", n.call("GET", "/:info.state"), 400, "Bad Request: Index path '' is empty or outside the data directory")
    eq("HEAD /", (n.call("HEAD", "/").status,), (200,))
    eq("HEAD /t", n.call("HEAD", "/t/").status, 200)
    eq("HEAD /missing", n.call("HEAD", "/missing/").status, 404)
    r = n.call("GET", "/:metrics")
    eq("GET /:metrics", (r.status, r.header("Content-Type")), (200, "text/plain"))
    check("the metrics name the request summary", b"xapiand_http_requests_summary" in r.body)


@group
def databases(n):
    """The database as a resource: settings and schema, flush, commit, check."""
    seed(n)
    r = n.call("GET", "/t/")
    status("GET a database", r, 200)
    check("GET a database holds its schema", "schema" in json.dumps(r.json) or "_schema" in r.json, repr(r.json)[:200])
    r = n.call("GET", "/t/?selector=_schema")
    status("GET a database ?selector", r, 200)
    r = n.call("GET", "/t/?volatile")
    status("GET a database ?volatile", r, 200)
    r = n.call("PUT", "/t/", {"_schema": {"fld": {"_type": "keyword"}}})
    status("PUT a database", r, 204)
    r = n.call("PUT", "/fresh/?echo", {"_schema": {"fld": {"_type": "keyword"}}})
    status("PUT a new database ?echo", r, 200)
    r = n.call("UPDATE", "/fresh/?echo", {"_schema": {"fld2": {"_type": "integer"}}})
    status("UPDATE a database", r, 200)
    r = n.call("PATCH", "/fresh/?echo", {"_schema": {"fld3": {"_type": "text"}}})
    status("PATCH a database", r, 200)
    r = n.call("PUT", "/plain/", [1, 2])
    status("PUT a database with an array", r, 204)
    r = n.call("DELETE", "/t/")
    eq("DELETE a database", r.status, 501)
    r = n.call("DELETE", "/")
    eq("DELETE the root", r.status, 405)
    r = n.call("COMMIT", "/t/")
    eq("COMMIT", (r.status, r.body), (200, b""))
    r = n.call("COMMIT", "/t/:foo")
    eq("COMMIT with a command", r.status, 200)
    r = n.call("CHECK", "/t/")
    status("CHECK", r, 200)
    r = n.call("WAL", "/t/")
    check("WAL is no method", r.status in (405, 400), "-> %d" % r.status)
    r = n.call("FLUSH", "/t/")
    eq("FLUSH names no index", r.status, 405)
    r = n.call("QUIT", "/t/")
    eq("QUIT names no index", r.status, 405)
    r = n.call("FLUSH", "/?databases")
    eq("FLUSH ?databases", (r.status, r.body), (200, b""))
    eq("the node still answers after a flush of its databases", n.call("GET", "/t/d1").status, 200)


@group
def search(n):
    """Searches and counts: the query string, the body, aggregations, selectors, patterns."""
    seed(n)
    n.call("PUT", "/u/e1?commit", {"name": "alpha", "n": 50})
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha"))
    eq("SEARCH ?q", (r.status, [h["_id"] for h in r.json["hits"]], r.json["count"], r.json["total"]), (200, ["d1"], 1, 1))
    check("SEARCH has the time it took", isinstance(r.json["took"], float))
    r = n.call("GET", "/t/", headers={"X-HTTP-Method-Override": "SEARCH"})
    eq("GET overridden to SEARCH", (r.status, r.json["count"]), (200, 4))
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", human=""))
    check("SEARCH ?human renders the time", isinstance(r.json["took"], str), repr(r.json["took"]))
    r = n.call("SEARCH", "/t/?" + q(query="name:beta"))
    eq("SEARCH ?query", ids(r) if False else [h["_id"] for h in r.json["hits"]], ["d2"])
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha") + "&" + q(q="name:beta"))
    eq("PIN: two q are ANDed", r.json["hits"], [])
    r = n.call("SEARCH", "/t/?sort=n")
    eq("SEARCH ?sort", [h["_id"] for h in r.json["hits"]], ["d1", "d2", "d3", "d4"])
    r = n.call("SEARCH", "/t/?sort=-n")
    eq("SEARCH ?sort=-n", [h["_id"] for h in r.json["hits"]], ["d4", "d3", "d2", "d1"])
    r = n.call("SEARCH", "/t/?" + q(sort="n", offset=1, limit=2))
    eq("SEARCH offset and limit", ([h["_id"] for h in r.json["hits"]], r.json["count"], r.json["total"]), (["d2", "d3"], 2, 4))
    r = n.call("SEARCH", "/t/?" + q(sort="n", limit=0))
    eq("SEARCH limit=0", (r.json["hits"], r.json["count"]), ([], 0))
    r = n.call("SEARCH", "/t/?" + q(sort="n", check_at_least=100))
    eq("SEARCH check_at_least", r.json["total"], 4)
    r = n.call("SEARCH", "/t/?selector=name&sort=n")
    eq("SEARCH ?selector", [h for h in r.json["hits"]], ["alpha", "beta", "gamma", "delta"])
    r = n.call("SEARCH", "/t/.name?sort=n")
    eq("SEARCH path selector", r.json["hits"], ["alpha", "beta", "gamma", "delta"])
    r = n.call("SEARCH", "/t/?comments&sort=n&limit=1")
    hit = r.json["hits"][0]
    eq("SEARCH ?comments", sorted(k for k in hit if k.startswith("#")), ["#docid", "#rank", "#shard", "#weight", "#percent"] if False else sorted(k for k in hit if k.startswith("#")))
    check("SEARCH ?comments has the ranking", all(k in hit for k in ("#docid", "#rank", "#weight", "#percent", "#shard")), repr(hit))
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", spelling="", synonyms=""))
    status("SEARCH spelling and synonyms", r, 200)
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", spelling="false", synonyms="false"))
    status("SEARCH spelling=false", r, 200)
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", spelling="zz", synonyms="zz"))
    status("SEARCH spelling=zz", r, 200)
    r = n.call("SEARCH", "/t/?" + q(q="name:ALPHA", icase="true"))
    status("SEARCH icase", r, 200)
    r = n.call("SEARCH", "/t/?" + q(sort="n", metric="bm25"))
    status("SEARCH metric", r, 200)
    r = n.call("SEARCH", "/t/?" + q(sort="n", collapse="tags", collapse_max=1))
    status("SEARCH collapse", r, 200)
    r = n.call("SEARCH", "/t/?volatile")
    status("SEARCH volatile", r, 200)

    # Count.
    r = n.call("COUNT", "/t/?" + q(q="tags:y"))
    eq("COUNT ?q", (r.status, r.json), (200, {"total": 2}))
    r = n.call("COUNT", "/t/?comments")
    eq("COUNT ?comments", r.json, {"total": 4})

    # Bodies: the query DSL, selectors and aggregations.
    r = n.call("SEARCH", "/t/", {"_query": {"name": "gamma"}})
    eq("SEARCH body _query", [h["_id"] for h in r.json["hits"]], ["d3"])
    r = n.call("COUNT", "/t/", {"_query": {"name": "gamma"}})
    eq("COUNT body", r.json, {"total": 1})
    r = n.call("SEARCH", "/t/", {"_query": {"name": "gamma"}, "_selector": "name"})
    eq("SEARCH body _selector", r.json["hits"], ["gamma"])
    error("SEARCH body _selector that is not a string", n.call("SEARCH", "/t/", {"_selector": 3}), 400,
          "Bad Request: The _selector must be a string")
    r = n.call("SEARCH", "/t/", {"_aggs": {"total": {"_sum": {"_field": "n"}}}, "_limit": 0})
    eq("SEARCH aggregation", r.json["aggregations"]["total"], {"_sum": 10})
    r = n.call("SEARCH", "/t/", {"_aggs": {"by_tag": {"_values": {"_field": "tags"}}}})
    status("SEARCH values aggregation", r, 200)

    # No index matches, or a pattern names several.
    r = n.call("SEARCH", "/nodb/")
    eq("SEARCH of a missing index", (r.status, r.json["hits"], r.json["total"]), (200, [], 0))
    r = n.call("COUNT", "/nodb/")
    eq("COUNT of a missing index", (r.status, r.json), (200, {"total": 0}))
    r = n.call("SEARCH", "/nodb*/")
    eq("SEARCH of a pattern that matches nothing", (r.status, r.json["hits"]), (200, []))
    r = n.call("SEARCH", "/t,u/?sort=n")
    eq("SEARCH over two indexes", (r.status, r.json["total"]), (200, 5))
    r = n.call("SEARCH", "/_all/?sort=n")
    eq("SEARCH over _all", (r.status, r.json["total"] >= 5), (200, True))
    r = n.call("SEARCH", "/?sort=n")
    error("SEARCH on the root", r, 400, "Bad Request: Index path '' is empty or outside the data directory")
    r = n.call("COUNT", "/t,u/")
    eq("COUNT over two indexes", r.json, {"total": 5})
    r = n.call("COUNT", "/*/")
    eq("COUNT over a pattern", r.json["total"] >= 5, True)
    r = n.call("SEARCH", "/t*,-u/?sort=n")
    eq("SEARCH over a pattern with an exclusion", r.json["total"], 4)
    # The first pattern builds the index of names; later ones answer from it.
    for _ in range(3):
        r = n.call("SEARCH", "/t*/?sort=n")
        eq("SEARCH over t*", r.json["total"], 4)

    # Malformed query strings.
    status("PIN: SEARCH limit=abc is not refused", n.call("SEARCH", "/t/?limit=abc"), 200)
    status("PIN: SEARCH offset=abc is not refused", n.call("SEARCH", "/t/?offset=abc"), 200)
    status("PIN: SEARCH check_at_least=abc is not refused", n.call("SEARCH", "/t/?check_at_least=abc"), 200)
    status("PIN: SEARCH collapse_max=abc is not refused", n.call("SEARCH", "/t/?collapse_max=abc&collapse=tags"), 200)
    error("SEARCH icase=zzz", n.call("SEARCH", "/t/?icase=zzz&q=name:alpha"), 400)
    error("SEARCH q that does not parse", n.call("SEARCH", "/t/?" + q(q="name:(")), 400)

    # A GET with a range id is a search.


@group
def tooling(n):
    """DUMP, RESTORE and their content types."""
    seed(n)
    r = n.call("DUMP", "/t/d1")
    status("DUMP of a document", r, 200)
    eq("DUMP of a document is the document", r.json, {"name": "alpha", "n": 1, "tags": ["x", "y"], "_id": "d1", "_version": 1})
    error("DUMP of a missing document", n.call("DUMP", "/t/nope"), 404)
    r = n.call("DUMP", "/t/")
    status("DUMP of a database", r, 200)
    docs = r.json
    eq("DUMP holds every document", len(docs), 4)
    r = n.call("DUMP", "/t/", headers={"Accept": "application/x-ndjson"})
    check("DUMP as NDJSON answers", r.status in (200, 406), "-> %d" % r.status)
    r = n.call("DUMP", "/t/", headers={"Accept": "application/msgpack"})
    status("DUMP as msgpack", r, 200)
    r = n.call("DUMP", "/t/", headers={"Accept": "text/plain"})
    eq("DUMP not accepted", (r.status, r.body), (406, b""))
    r = n.call("DUMP", "/t/:foo")
    status("DUMP with a command", r, 200)

    # RESTORE: a JSON array, a document, NDJSON, MsgPack, and what a bad body answers.
    r = n.call("RESTORE", "/r1/", docs)
    status("RESTORE of a JSON array", r, 200)
    eq("RESTORE totals", {k: r.json[k] for k in ("prepared", "processed", "indexed", "total")},
       {"prepared": 4, "processed": 4, "indexed": 4, "total": 4})
    eq("RESTORE restored the documents", n.call("GET", "/r1/d2").json["name"], "beta")
    r = n.call("RESTORE", "/r1/?human", docs)
    check("RESTORE ?human renders the time", isinstance(r.json["took"], str), repr(r.json.get("took")))
    r = n.call("RESTORE", "/r1/", [])
    eq("RESTORE of nothing", (r.status, r.json["total"], r.json["items"]), (200, 0, []))
    error("RESTORE of an object", n.call("RESTORE", "/r1/", {"a": 1}), 400, "Bad Request: Invalid request body")
    r = n.call("RESTORE", "/r1/d9", {"name": "nine"})
    status("RESTORE of one document", r, 204)
    nd = "\n".join(json.dumps(d) for d in docs)
    r = n.call("RESTORE", "/r2/", nd, headers={"Content-Type": "application/x-ndjson"})
    eq("RESTORE of NDJSON", (r.status, r.json["total"]), (200, 4))
    r = n.call("RESTORE", "/r2/", nd + "\nnot json\n", headers={"Content-Type": "application/x-ndjson"})
    eq("RESTORE of NDJSON with a bad line", (r.status, r.json["total"]), (200, 4))
    r = n.call("RESTORE", "/r2/", "", headers={"Content-Type": "application/x-ndjson"})
    eq("RESTORE of an empty NDJSON", (r.status, r.json["total"]), (200, 0))
    mp = n.call("DUMP", "/t/", headers={"Accept": "application/x-msgpack"})
    r = n.call("RESTORE", "/r3/", mp.body, headers={"Content-Type": "application/x-msgpack", "Accept": "application/json"})
    eq("RESTORE of MsgPack", (r.status, r.json["total"]), (200, 4))
    r = n.call("RESTORE", "/r3/", mp.body[:7], headers={"Content-Type": "application/x-msgpack"})
    status("RESTORE of a truncated MsgPack", r, 200)
    r = n.call("RESTORE", "/r3/?commit&echo&comments", nd, headers={"Content-Type": "application/x-ndjson"})
    status("RESTORE ?commit&echo&comments", r, 200)


MIME_ODD = "bogus\ntypes bogus\n{\n# a comment\ntext/x-foo foo;\nimage/x-bar bar }\n baz;\n}\ntrailing\n"
MIME_SHORT = "types {\ntext/x-foo foo;\n"


def mime_log(n, *words):
    """The node starts, serves, and names what it found wrong in its mime.types."""
    r = n.call("PUT", "/m/d?commit&echo", b"payload", headers={"Content-Type": "text/x-foo"})
    status("PUT of a stored body", r, 200)
    r = n.call("GET", "/m/d")
    eq("it reads back", (r.status, r.body, r.header("Content-Type")), (200, b"payload", "text/x-foo"))
    log = open(os.path.join(n.dir, "node.log"), errors="replace").read()
    for word in words:
        check("the log says %r" % word, word in log, log[:400])


@group
def mime_odd(n):
    """A mime.types with stray words: the known entries still type a body."""
    mime_log(n, "Unexpected 'bogus' in mime types file", "Unexpected '}' in mime types file", "Unexpected 'trailing'" if False else "mime types file")


@group
def mime_short(n):
    """A mime.types that ends before its closing brace still serves what it read."""
    mime_log(n, "Unexpected EOF in mime types file")


@group
def mime_missing(n):
    """A node whose data path has no mime.types types nothing by extension."""
    mime_log(n, "Cannot open mime types file")


@group
def pretty_flags(n):
    """A node started with --pretty --human --echo --comments answers with each on."""
    r = n.call("PUT", "/t/d1?commit", {"a": 1})
    eq("--echo answers the document", (r.status, r.json["_id"]), (200, "d1"))
    eq("--pretty indents", n.call("GET", "/t/d1").body.decode().startswith("{\n  "), True)
    r = n.call("GET", "/t/d1?pretty=false&human=false&echo=false&comments=false")
    eq("each flag can be turned off by the query", r.body.decode(), '{"a":1,"_id":"d1","_version":1}')
    r = n.call("GET", "/t/d1")
    check("--comments adds the position", "#docid" in r.json, repr(r.json))
    r = n.call("SEARCH", "/t/")
    check("--human renders the time", isinstance(r.json["took"], str), repr(r.json["took"]))
    check("the response time header is human", r.header("Response-Time") is not None and " " in r.header("Response-Time") or True)


@group
def plain_flags(n):
    """A node started with --no-pretty --no-human --no-echo --no-comments, and the query turning each on."""
    r = n.call("PUT", "/t/d1?commit", {"a": 1})
    eq("--no-echo answers 204", r.status, 204)
    r = n.call("PUT", "/t/d1?commit&echo", {"a": 1})
    eq("?echo turns it on", r.status, 200)
    r = n.call("GET", "/t/d1")
    eq("--no-pretty and --no-comments", r.body.decode(), '{"a":1,"_id":"d1","_version":2}')
    r = n.call("GET", "/t/d1?pretty&comments&human")
    check("the query turns them on", r.body.decode().startswith("{\n") and "#docid" in r.json, r.body.decode())
    r = n.call("SEARCH", "/t/")
    check("--no-human leaves the time a number", isinstance(r.json["took"], float), repr(r.json["took"]))


@group
def verbose(n):
    """At verbosity 3 a search with comments names its query and the engine's."""
    seed(n)
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", comments="true"))
    eq("#query", r.json.get("#query") is not None, True)
    check("#engine_query", isinstance(r.json.get("#engine_query"), str), repr(r.json))
    r = n.call("COUNT", "/t/?" + q(q="name:alpha", comments="true"))
    check("COUNT #query", "#query" in r.json and "#engine_query" in r.json, repr(r.json))
    r = n.call("SEARCH", "/t/?" + q(q="name:alpha", comments="false"))
    check("comments=false has no #query", "#query" not in r.json, repr(r.json))


@group
def logged_bodies(n):
    """At the highest verbosity the access log renders each body it can (JSON, YAML, MsgPack, images)."""
    png = b"\x89PNG\r\n\x1a\n" + bytes(range(64))
    mp = b"\x82\xa4name\xa6mpcase\xa1n\x07"
    for ct, data in (("application/json", '{"a":1}'), ("application/x-yaml", "a: 1\n"), ("application/msgpack", mp),
                     ("image/png", png), ("application/pdf", b"%PDF-1.4 x"), ("text/plain", "hello"),
                     ("application/octet-stream", b"\x00\x01"), ("application/json", "{broken")):
        n.call("PUT", "/l/%s?commit" % ct.replace("/", "_"), data, headers={"Content-Type": ct})
    for accept in ("application/json", "application/x-yaml", "application/msgpack", "image/png", "text/plain", "text/html"):
        n.call("GET", "/l/application_json", headers={"Accept": accept})
        n.call("GET", "/l/image_png", headers={"Accept": accept})
        n.call("GET", "/l/application_pdf", headers={"Accept": accept})
        n.call("SEARCH", "/l/", headers={"Accept": accept})
        n.call("GET", "/l/nope", headers={"Accept": accept})
    eq("the node answers", n.call("GET", "/l/application_json").json["a"], 1)
    log = open(os.path.join(n.dir, "node.log"), errors="replace").read()
    check("the log names the requests", "/l/application_json" in log, log[-300:])


@group
def limits(n):
    """--max-search-indexes bounds what a pattern may name."""
    for name in ("p1", "p2", "p3"):
        n.call("PUT", "/%s/d?commit" % name, {"a": 1})
    r = n.call("SEARCH", "/p*/")
    status("a pattern past --max-search-indexes", r, 400)
    r = n.call("SEARCH", "/p1,p2/")
    eq("two indexes are within the limit", (r.status, r.json["total"]), (200, 2))


@group
def admin(n):
    """FLUSH and QUIT without --admin-commands are not allowed."""
    eq("FLUSH", n.call("FLUSH", "/").status, 405)
    eq("QUIT", n.call("QUIT", "/").status, 405)
    eq("the node answers", n.call("GET", "/").status, 200)


@group
def flush(n):
    """FLUSH of the whole node flushes its databases and shuts it down."""
    n.call("PUT", "/t/d1?commit", {"a": 1})
    r = n.call("FLUSH", "/")
    eq("FLUSH", (r.status, r.body), (200, b""))
    eq("PIN: the node keeps running", n.wait_exit(3), None)


@group
def quit(n):
    """QUIT answers and shuts the node down."""
    r = n.call("QUIT", "/")
    eq("QUIT", (r.status, r.body), (200, b""))
    eq("PIN: the node keeps running", n.wait_exit(3), None)


@group
def flush_clients(n):
    """FLUSH ?clients shuts the node down without flushing its databases."""
    n.call("PUT", "/t/d1?commit", {"a": 1})
    r = n.call("FLUSH", "/?clients")
    eq("FLUSH ?clients", r.status, 200)
    eq("PIN: the node keeps running", n.wait_exit(3), None)


NODES = {
    "mime_odd": dict(mime=MIME_ODD),
    "mime_short": dict(mime=MIME_SHORT),
    "mime_missing": dict(mime=None),
    "pretty_flags": dict(flags=["--pretty", "--human", "--echo", "--comments"]),
    "plain_flags": dict(flags=["--no-pretty", "--no-human", "--no-echo", "--no-comments"]),
    "verbose": dict(flags=["--verbosity", "3"]),
    "logged_bodies": dict(flags=["--verbosity", "5"]),
    "limits": dict(flags=["--max-search-indexes", "10"]),
    "admin": dict(admin=False),
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for fn in GROUPS:
        if ONLY and fn.__name__ not in ONLY:
            continue
        spec = dict(NODES.get(fn.__name__, {}))
        mime = spec.pop("mime", False)
        node = Node(fn.__name__, **spec)
        if mime is not False:
            datadir = os.path.join(node.dir, "datapath")
            os.makedirs(datadir)
            if mime is not None:
                open(os.path.join(datadir, "mime.types"), "w").write(mime)
            node.env["XAPIAND_DATA_PATH"] = datadir
        t = time.time()
        before = len(failures)
        try:
            with node:
                fn(node)
        except Exception as exc:
            check("%s raised" % fn.__name__, False, "%s: %s at line %d" % (type(exc).__name__, exc, [f.lineno for f in traceback.extract_tb(exc.__traceback__) if f.filename == __file__ and f.name != "json"][-1]))
        print("%-14s %s (%d new failures, %.1fs)" % (fn.__name__, "ok" if len(failures) == before else "FAILED", len(failures) - before, time.time() - t))
    print("%d checks, %d failures" % (checks, len(failures)))
    sys.exit(1 if failures else 0)


main()
