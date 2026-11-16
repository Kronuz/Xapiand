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
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl

MSGPACK_CT = "application/x-msgpack"


def lines(objs: list[dict]) -> bytes:
    return b"".join(json.dumps(o).encode() + b"\n" for o in objs)


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
    return out


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
