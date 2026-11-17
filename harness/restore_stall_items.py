#!/usr/bin/env python3
"""Whether two xapiand binaries answer the same RESTOREs with the same bytes: each binary gets a node of its own and
the same sequence of RESTOREs (streamed and array bodies; given, new numeric and repeated ids; one and three shards;
echo, comments and neither; objects that fail their prepare; a script object, prepared on the loop), in JSON and in
MessagePack. The responses' `took` is blanked; every other byte must match. Exits 0 when all match.

  harness/restore_stall_items.py BASE_BIN HEAD_BIN [--workdir DIR]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl

MSGPACK_CT = "application/x-msgpack"


def lines(objs: list[dict]) -> bytes:
    return b"".join(json.dumps(o).encode() + b"\n" for o in objs)


class Pairs:
    """A map as ordered pairs, for repeated or non-string keys."""

    def __init__(self, items: list[tuple]) -> None:
        self.items = items


def mp(o: object) -> bytes:
    """A small MessagePack encoder, enough for the awkward bodies."""
    if o is None or isinstance(o, bool):
        return b"\xc0" if o is None else (b"\xc3" if o else b"\xc2")
    if isinstance(o, int):
        if 0 <= o < 128:
            return bytes([o])
        if -32 <= o < 0:
            return struct.pack("b", o)
        if o >= 0:
            return b"\xcf" + struct.pack(">Q", o) if o >= 2**32 else b"\xce" + struct.pack(">I", o)
        return b"\xd3" + struct.pack(">q", o)
    if isinstance(o, float):
        return b"\xcb" + struct.pack(">d", o)
    if isinstance(o, str):
        b = o.encode()
        n = len(b)
        if n < 32:
            return bytes([0xA0 | n]) + b
        return (b"\xd9" + bytes([n]) if n < 256 else b"\xda" + struct.pack(">H", n)) + b
    if isinstance(o, list):
        return (bytes([0x90 | len(o)]) if len(o) < 16 else b"\xdc" + struct.pack(">H", len(o))) + b"".join(
            mp(x) for x in o
        )
    pairs = o if isinstance(o, Pairs) else Pairs(list(o.items()))  # type: ignore[attr-defined]
    n = len(pairs.items)
    return (bytes([0x80 | n]) if n < 16 else b"\xde" + struct.pack(">H", n)) + b"".join(
        mp(k) + mp(v) for k, v in pairs.items
    )


def awkward(fmt: str) -> list[tuple[str, str, bytes, str]]:
    """Bodies that stress the offset and size bookkeeping of the results: repeated keys, `_id` of every type and size,
    `_op_type` variants, script and schema objects, MessagePack bodies (repeated and non-string keys), offset and limit,
    nested arrays, streams that break halfway, and results past the array16 and array32 headers. Every object has an
    `_id`, so the responses are the same on both binaries."""
    ix = lambda name, q="?commit&echo": f"/{fmt}-awk-{name}/" + q
    odd = [
        "1.5",
        "true",
        "null",
        "[1,2]",
        '{"a":1}',
        "-5",
        "18446744073709551615",
        '""',
        '"é…✓"',
        '"' + "x" * 300 + '"',
        '"' + "y" * 70000 + '"',
        "-9223372036854775808",
        "0",
        "31",
        "32",
        "127",
        "128",
        "255",
        "256",
        "65535",
        "65536",
        "4294967295",
        "4294967296",
    ]
    idn = lambda ids: b"".join(f'{{"_id":{v},"n":1}}\n'.encode() for v in ids)
    nd = lambda *ls: b"".join(x.encode() + b"\n" for x in ls)
    mps = lambda objs: b"".join(mp(o) for o in objs)
    big = lambda n: nd(*[f'{{"_id":"d{i}"}}' for i in range(n)])
    nd_ct, mp_ct = sl.NDJSON_CT, MSGPACK_CT
    return [
        (
            "dup-top",
            ix("dup-top"),
            nd(
                '{"_id":"d1","n":1,"n":2,"t":"a"}',
                '{"_id":"d2","_id":"d3","n":5}',
                '{"n":7,"_id":"d4","n":8,"_id":"d5"}',
            ),
            nd_ct,
        ),
        (
            "dup-op-type",
            ix("dup-op"),
            nd(
                '{"_id":"f1","_op_type":"delete","_op_type":"index","n":1}',
                '{"_id":"f2","_op_type":"index","_op_type":"delete"}',
            ),
            nd_ct,
        ),
        ("dup-script", ix("dup-script"), nd('{"_id":"g1","n":1,"_script":"_doc.n = 5","_script":"_doc.n = 6"}'), nd_ct),
        ("odd-ids", ix("odd-ids"), idn(odd), nd_ct),
        ("odd-ids-comments", ix("odd-ids-c", "?commit&echo&comments"), idn(odd[:9]), nd_ct),
        (
            "op-types",
            ix("op-types"),
            nd(
                '{"_id":"i1","_op_type":"index","n":1}',
                '{"_id":"i2","_op_type":"delete"}',
                '{"_id":"i4","_op_type":5,"n":1}',
                '{"_id":"i5","_op_type":"update","n":1}',
                '{"_id":"i6","_op_type":null}',
                '{"_id":"i7","_op_type":"create","n":2}',
                '{"_id":"i8","_op_type":"INDEX","n":2}',
                '{"_id":"i9","_op_type":["index"]}',
            ),
            nd_ct,
        ),
        (
            "script-schema",
            ix("script-schema"),
            nd(
                '{"_id":"j1","n":1}',
                '{"_id":"j2","_script":"_doc.n = 2","n":0}',
                '{"_id":"j3","n":3}',
                '{"_id":"j4","_schema":{"n":{"_type":"integer"}},"n":4}',
                '{"_id":"j5","n":5}',
                '{"_id":"j6","_script":"bad (","n":1}',
                '{"_id":"j7","_schema":5,"n":1}',
                '{"_id":"j8","_script":5,"n":1}',
            ),
            nd_ct,
        ),
        (
            "nested-arrays",
            ix("nested"),
            nd(
                '[{"_id":"k1","n":1},{"_id":"k2","n":2}]',
                '[1,{"_id":"k3","n":3},"x",{"_id":"k4","n":4}]',
                "[]",
                '[[{"_id":"k5"}]]',
                "5",
                '"s"',
                '{"_id":"k6","n":6}',
            ),
            nd_ct,
        ),
        (
            "long-error-id",
            ix("long-err"),
            nd(
                '{"_id":"m1","n":"zz"}', '{"_id":"' + "m" * 5000 + '","n":"zz"}', '{"_id":"m3","n":"' + "q" * 400 + '"}'
            ),
            nd_ct,
        ),
        (
            "offset-limit",
            ix("offlim", "?commit&echo&offset=3&limit=5"),
            nd(*[f'{{"_id":"o{i}","n":{i}}}' for i in range(30)]),
            nd_ct,
        ),
        ("limit-0", ix("lim0", "?commit&echo&limit=0"), nd(*[f'{{"_id":"p{i}","n":{i}}}' for i in range(10)]), nd_ct),
        (
            "array-dups",
            ix("arr-dups"),
            json.dumps([{"_id": "r1", "n": 1}, {"_id": "r1", "n": 2}, {"_id": "r1", "n": 3}]).encode(),
            sl.JSON_CT,
        ),
        ("mp-basic", ix("mp-basic"), mps([{"_id": f"t{i}", "n": i, "t": "mp"} for i in range(30)]), mp_ct),
        (
            "mp-dups",
            ix("mp-dups"),
            mp(Pairs([("_id", "u1"), ("n", 1), ("n", 2), ("_id", "u2")])) + mp({"_id": "u3", "n": 3}),
            mp_ct,
        ),
        (
            "mp-script",
            ix("mp-script"),
            mps([{"_id": "v1", "n": 1}, {"_id": "v2", "_script": "_doc.n = 3", "n": 0}, {"_id": "v3", "n": 3}]),
            mp_ct,
        ),
        (
            "mp-odd-ids",
            ix("mp-odd"),
            mps(
                [
                    {"_id": v, "n": 1}
                    for v in [1.5, True, None, [1, 2], {"a": 1}, -5, 2**64 - 1, "", "x" * 300, 2**32, -33, -(2**40)]
                ]
            ),
            mp_ct,
        ),
        ("mp-bad-key-first", ix("mp-badkey1"), mp(Pairs([(1, "a"), ("_id", "w1")])) + mp({"_id": "w2", "n": 2}), mp_ct),
        (
            "mp-bad-key-mid",
            ix("mp-badkey2"),
            mps([{"_id": f"x{i}", "n": i} for i in range(10)])
            + mp(Pairs([(5, "a"), ("_id", "xbad")]))
            + mps([{"_id": f"y{i}", "n": i} for i in range(10)]),
            mp_ct,
        ),
        (
            "break-json",
            ix("break-json"),
            nd(
                *[f'{{"_id":"z{i}","n":{i}}}' for i in range(200)],
                '{"_id":"zbad","n":',
                *[f'{{"_id":"zz{i}","n":{i}}}' for i in range(50)],
            ),
            nd_ct,
        ),
        (
            "break-mp",
            ix("break-mp"),
            mps([{"_id": f"ta{i}", "n": i} for i in range(100)]) + mp({"_id": "tb", "n": 1})[:-3],
            mp_ct,
        ),
        ("n16", ix("n16", "?commit"), big(16), nd_ct),
        ("n12000", ix("n12000", "?commit"), big(12000), nd_ct),
        ("n70000", ix("n70000", "?commit"), big(70000), nd_ct),  # a results array32 header
    ]


def cases(fmt: str) -> list[tuple[str, str, bytes, str]]:
    """(name, path, body, content type), in order: later ones see what earlier ones wrote; `fmt` prefixes the indexes."""
    given = [{"_id": f"k{i}", "n": i, "t": f"text {i}"} for i in range(300)]
    numeric = [{"n": i, "t": f"new {i}"} for i in range(200)]
    repeated = [{"_id": f"k{i % 7}", "n": i} for i in range(50)]
    failing = [{"_id": f"bad{i}", "n": "not a number"} if i % 5 == 0 else {"_id": f"ok{i}", "n": i} for i in range(60)]
    scripted = [{"_id": "s1", "n": 1}, {"_id": "s2", "_script": "_doc.n = 2", "n": 0}, {"_id": "s3", "n": 3}]
    integer = [{"_id": 1, "n": 0}]  # integer ids: the objects without one get new numeric ids (UUIDs would differ)
    settings3 = {"_id": "settings", "_settings": {"number_of_shards": 3, "number_of_replicas": 0}}
    out: list[tuple[str, str, bytes, str]] = []
    for tag, q in (("default", ""), ("echo", "&echo"), ("comments", "&echo&comments"), ("quiet", "&echo=false")):
        one, num, arr, three, num3 = (
            f"/{fmt}-{name}-{tag}/?commit{q}" for name in ("one", "num", "arr", "three", "num3")
        )
        out += [
            (f"given-{tag}", one, lines(given), sl.NDJSON_CT),
            (f"numeric-{tag}", num, lines(integer + numeric), sl.NDJSON_CT),
            (f"repeated-{tag}", one, lines(repeated), sl.NDJSON_CT),
            (f"failing-{tag}", one, lines(failing), sl.NDJSON_CT),
            (f"scripted-{tag}", one, lines(scripted), sl.NDJSON_CT),
            (f"array-{tag}", arr, json.dumps(given[:40]).encode(), sl.JSON_CT),
            (f"sharded-{tag}", three, lines([settings3, *given, *repeated, *failing]), sl.NDJSON_CT),
            (f"sharded-numeric-{tag}", num3, lines([{**settings3, "_id": 1}, *numeric]), sl.NDJSON_CT),
        ]
    return out + [(f"awk-{n}", p, b, ct) for n, p, b, ct in awkward(fmt)]


class AcceptConn(sl.Conn):
    """A connection whose requests ask for `accept`."""

    accept = sl.JSON_CT

    def head(self, method: str, path: str, length: int, content_type: str = sl.JSON_CT) -> bytes:
        return super().head(method, path, length, content_type)[:-2] + f"Accept: {self.accept}\r\n\r\n".encode()


async def run(bin_path: str, workdir: str, port: int) -> dict[str, bytes]:
    ns = argparse.Namespace(target=f"127.0.0.1:{port}", bin=bin_path, workdir=workdir, processors=1, index_size="")
    node = sl.Node(ns)
    got: dict[str, bytes] = {}
    try:
        conn = AcceptConn(node.host, node.port)
        for fmt, accept in (("json", sl.JSON_CT), ("msgpack", MSGPACK_CT)):
            conn.accept = accept
            for name, path, body, ct in cases(fmt):
                status, data = await conn.request("RESTORE", path, body, ct, timeout=120)
                got[f"{fmt}:{name}"] = b"%d " % status + blank_took(data)
        await conn.close()
    finally:
        node.stop()
    return got


def blank_took(data: bytes) -> bytes:
    data = re.sub(rb'"took":\s*[0-9.eE+-]+', b'"took":0', data)
    i = data.find(b"\xa4took\xcb")  # MessagePack: "took" then a float64
    if i >= 0:
        data = data[: i + 6] + b"\0" * 8 + data[i + 14 :]
    return data


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base")
    ap.add_argument("head")
    ap.add_argument("--workdir", default=os.path.join(sl.REPO, ".scratch", "restore_items"))
    ap.add_argument("--port", type=int, default=18331)
    args = ap.parse_args()
    base = asyncio.run(run(args.base, args.workdir, args.port))
    head = asyncio.run(run(args.head, args.workdir, args.port))
    same = 0
    for key in base:
        if base[key] == head.get(key):
            same += 1
        else:
            print(f"DIFF {key}:\n  base {base[key][:400]!r}\n  head {head.get(key, b'')[:400]!r}")
    print(f"{same} of {len(base)} RESTORE responses byte-identical")
    statuses = sorted({v.split(b" ", 1)[0].decode() for v in base.values()})
    failed = sum(v.count(b'"status":') for k, v in base.items() if k.startswith("json:"))
    print(f"response statuses {statuses}; {failed} failed items among the JSON responses")
    return 0 if same == len(base) else 1


if __name__ == "__main__":
    raise SystemExit(main())
