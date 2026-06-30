#!/usr/bin/env python3
"""
bodydiff.py -- response-body differential diff between two newman JSON runs.

The doc suite's assertions are shallow ("Response is success"); this compares the
actual response *bodies* between a baseline run and ours, after normalizing the
volatile bits (internal doc ids, timestamps, took/elapsed, result ordering). Any
residual divergence is a behavior difference the assertions would miss -- exactly
the kind of thing the Leg 2 serialization changes could introduce.

  python3 bodydiff.py baseline.json ours.json
"""
import json
import re
import sys

# Keys whose values vary run-to-run and aren't part of API semantics.
# `endpoint` embeds the random per-boot node name (xapian://<node>/...); auto-ids
# (~xxxx) and uuids/timestamps differ per run.
VOLATILE_KEYS = {
    "#docid", "#shard", "#start", "took", "time", "took_ms", "elapsed",
    "endpoint", "_endpoint", "_node", "node", "processed", "indexed", "total_count",
    "offset",  # internal storage position of a stored blob (shifts with codec); size is stable
}
# Boot both nodes with a fixed --name so node-identity fields (name, _id of the
# node doc, prometheus node= labels) match; otherwise the random per-boot node
# name shows up as spurious divergences in GET / , /.nodes and /:metrics.
UUID_RE = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
TS_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T[\d:.]+")
AUTOID_RE = re.compile(r"^~[A-Za-z0-9_-]{6,}$")


def normalize(obj):
    if isinstance(obj, dict):
        out = {}
        for k, v in obj.items():
            if k in VOLATILE_KEYS:
                continue
            out[k] = normalize(v)
        return out
    if isinstance(obj, list):
        items = [normalize(v) for v in obj]
        # result/hit ordering can differ on score ties; compare as a set-ish bag
        try:
            return sorted(items, key=lambda x: json.dumps(x, sort_keys=True))
        except Exception:
            return items
    if isinstance(obj, str):
        if AUTOID_RE.match(obj):
            return "<autoid>"
        if UUID_RE.match(obj):
            return "<uuid>"
        if TS_RE.match(obj):
            return "<ts>"
    return obj


def body_of(execution):
    st = (execution.get("response") or {}).get("stream")
    code = (execution.get("response") or {}).get("code")
    raw = ""
    if isinstance(st, dict) and "data" in st:
        raw = bytes(st["data"]).decode("utf-8", "replace")
    try:
        return code, normalize(json.loads(raw))
    except Exception:
        return code, raw  # non-JSON body, compare raw


def label(execution):
    url = (execution.get("request") or {}).get("url") or {}
    path = "/" + "/".join(url.get("path", []) or []) if isinstance(url, dict) else str(url)
    return f"{(execution.get('request') or {}).get('method','?')} {path}"


base = json.load(open(sys.argv[1]))["run"]["executions"]
ours = json.load(open(sys.argv[2]))["run"]["executions"]
n = min(len(base), len(ours))

diffs = []
for i in range(n):
    bc, bb = body_of(base[i])
    oc, ob = body_of(ours[i])
    if bc != oc or bb != ob:
        diffs.append((label(ours[i]), bc, oc, bb, ob))

print(f"compared {n} response bodies (normalized)\n")
print(f"BODY DIVERGENCES: {len(diffs)}\n")
for lbl, bc, oc, bb, ob in diffs[:40]:
    if bc != oc:
        print(f"  {lbl}  status {bc} -> {oc}")
    else:
        bs = json.dumps(bb, sort_keys=True)
        os_ = json.dumps(ob, sort_keys=True)
        print(f"  {lbl}  body differs:")
        print(f"      base: {bs[:160]}")
        print(f"      ours: {os_[:160]}")

print("\n=== VERDICT ===")
print("BODIES MATCH (normalized): no behavioral divergence." if not diffs
      else f"{len(diffs)} bodies diverge -- inspect (some may be volatile fields to add to the normalizer).")
sys.exit(1 if diffs else 0)
