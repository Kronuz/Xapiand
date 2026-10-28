#!/usr/bin/env python3
"""fields_identity.py BIN OUTDIR TAG: what an engine seam change must leave alone, as files to diff.

Starts BIN (--solo, a fresh data directory under OUTDIR, fixed node name), indexes bank (accounts.ndjson)
and cities (cities.ndjson) the way the docs suite does, runs a battery of SEARCH requests, reads every
document's `:info` (its terms with frequencies and positions), commits and stops the node, and dumps every
shard's terms with `xapian-delve`. Writes, under OUTDIR:

  TAG.search.jsonl   one line per query: the request and the normalized response
  TAG.info.jsonl     one line per document: its `:info` body, normalized
  TAG.terms          one line per (shard:docid, wdf, term in hex), sorted

and prints the sha256 of each. Run it on two binaries and compare the hashes (or diff the files).
`ASSETS` points at docs/public/assets, `PORT` moves the node (default 18951).
"""
import hashlib
import http.client
import json
import os
import re
import shutil
import subprocess
import sys
import time

BIN, OUT, TAG = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3]
PORT = int(os.environ.get("PORT", "18951"))
ASSETS = os.environ.get("ASSETS", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "public", "assets"))
VOLATILE = {"took", "time", "duration", "_took", "uptime"}

BANK = [
    {"_query": {"favoriteFruit": "banana"}},
    {"_query": {"favoriteFruit": "ban*"}},
    {"_query": {"favoriteFruit": "banan~"}},
    {"_query": {"favoriteFruit": "ba?an*"}},
    {"_query": {"favoriteFruit": None}},
    {"_query": {"favoriteFruit": "*"}},
    {"_query": {"name": {"firstName": "Michael"}}},
    {"_query": {"name.lastName": "lee"}},
    {"_query": {"age": 24}},
    {"_query": {"age": {"_in": {"_range": {"_from": 20, "_to": 30}}}}},
    {"_query": {"balance": {"_in": {"_range": {"_from": 100, "_to": 150}}}}},
    {"_query": {"accountNumber": {"_in": {"_range": {"_from": 100000, "_to": 200000}}}}},
    {"_query": {"personality": "love pizza"}},
    {"_query": {"personality": "lovely"}},
    {"_query": {"personality": "lov*"}},
    {"_query": {"personality": "lov~"}},
    {"_query": {"contact.state": "Indiana"}},
    {"_query": {"contact": {"city": "Leyner"}}},
    {"_query": {"contact.email": "michael.lee@globoil.co.uk"}},
    {"_query": {"style.clothing.pants": "khakis"}},
    {"_query": {"style.hairstyle": "slick back"}},
    {"_query": {"style": "*"}},
    {"_query": {"checkin": {"_in": {"_circle": {"_longitude": -95.63, "_latitude": 31.76, "_radius": 2000000}}}}},
    {"_query": {"checkin": "*"}},
    {"_query": {"gender": "male", "eyeColor": "blue"}},
    {"_query": {"_and": [{"gender": "female"}, {"age": {"_in": {"_range": {"_from": 30, "_to": 40}}}}]}},
    {"_query": {"_or": [{"eyeColor": "green"}, {"favoriteFruit": "lemon"}]}},
    {"_query": {"_not": {"gender": "male"}}},
    {"_query": "*"},
    {"_query": {"_id": 1}},
    {"_query": {"_id": {"_in": {"_range": {"_from": 100, "_to": 120}}}}},
    {"_query": "Michael lee"},
    {"_query": {"employer": "Globoil"}},
    {"_query": {"employer": "glob*"}},
]
CITIES = [
    {"_query": {"city": "Aliso Viejo"}},
    {"_query": {"city": "san*"}},
    {"_query": {"state": "California"}},
    {"_query": {"population": {"_in": {"_range": {"_from": 50000, "_to": 60000}}}}},
    {"_query": {"location": {"_in": {"_circle": {"_longitude": -122.4, "_latitude": 37.6, "_radius": 50000}}}}},
    {"_query": "*"},
]


def call(method, path, body=None):
    c = http.client.HTTPConnection("localhost", PORT, timeout=120)
    c.request(method, path, body=body, headers={"Content-Type": "application/json", "Accept": "application/json"})
    r = c.getresponse()
    return r.status, r.read()


def strip(value):
    if isinstance(value, dict):
        return {k: strip(v) for k, v in value.items() if k not in VOLATILE}
    if isinstance(value, list):
        return [strip(v) for v in value]
    return value


def normal(raw):
    try:
        return json.dumps(strip(json.loads(raw)), sort_keys=True)
    except ValueError:
        return raw.decode("utf-8", "replace")


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main():
    os.makedirs(OUT, exist_ok=True)
    data = os.path.join(OUT, f"data-{TAG}")
    shutil.rmtree(data, ignore_errors=True)
    node = subprocess.Popen([BIN, "--port", str(PORT), "--name", "fknode", "--solo", "-D", data], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(240):
            try:
                if call("GET", "/")[0] == 200:
                    break
            except OSError:
                time.sleep(0.5)
        counts = {}
        for name, corpus in (("bank", "accounts.ndjson"), ("cities", "cities.ndjson")):
            lines = open(os.path.join(ASSETS, corpus)).read().splitlines()
            counts[name] = len(lines)
            for n, line in enumerate(lines, 1):
                status, body = call("PUT", f"/{name}/{n}?commit", line)
                assert status in (200, 201, 204), (name, n, status, body[:200])
        search_path = os.path.join(OUT, f"{TAG}.search.jsonl")
        with open(search_path, "w") as f:
            for index, battery in (("bank", BANK), ("cities", CITIES)):
                for query in battery:
                    for extra in ("", "?sort=_id"):
                        body = dict(query, _limit=1000, _check_at_least=1000)
                        status, raw = call("SEARCH", f"/{index}/{extra}", json.dumps(body))
                        f.write(json.dumps([index, extra, query, status, normal(raw)], sort_keys=True) + "\n")
        info_path = os.path.join(OUT, f"{TAG}.info.jsonl")
        with open(info_path, "w") as f:
            for index in ("bank", "cities"):
                for n in range(1, counts[index] + 1):
                    status, raw = call("GET", f"/{index}/{n}:info")
                    f.write(json.dumps([index, n, status, normal(raw)], sort_keys=True) + "\n")
    finally:
        node.terminate()
        node.wait(timeout=60)
    terms_path = os.path.join(OUT, f"{TAG}.terms")
    shards = sorted(r for r, _, fs in os.walk(data) if "iamglass" in fs and re.search(r"/(bank|cities)/", r + "/"))
    # delve prints each term raw, then " wdf termfreq" (the term may hold any byte, so match from the end)
    term_re = re.compile(rb"(.*?) (\d+) (\d+)\n", re.DOTALL)
    rows = []
    for shard in shards:
        rel = os.path.relpath(shard, data)
        info = subprocess.run(["xapian-delve", shard], capture_output=True, text=True, check=True).stdout
        last = int(re.search(r"highest document id ever used = (\d+)", info).group(1))
        for n in range(1, last + 1):
            out = subprocess.run(["xapian-delve", "-1", "-v", "-r", str(n), shard], capture_output=True, check=False).stdout
            body = out.split(b"\n", 1)[1] if out.startswith(b"Term List") else b""
            for m in term_re.finditer(body):
                rows.append(f"{rel}:{n}\t{int(m.group(2))}\t{m.group(1).hex()}")
    with open(terms_path, "w") as f:
        f.write("\n".join(sorted(rows)) + "\n")
    for path in (search_path, info_path, terms_path):
        print(f"{sha(path)}  {os.path.relpath(path)}  ({sum(1 for _ in open(path))} lines)")


if __name__ == "__main__":
    main()
