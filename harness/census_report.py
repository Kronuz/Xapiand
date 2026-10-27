#!/usr/bin/env python3
"""census_report.py CENSUS.jsonl [NEWMAN.json] -- summarise an ENGINE_CENSUS capture.

CENSUS.jsonl is the file the node wrote ($XAPIAND_ENGINE_CENSUS); the newman report, when given,
supplies each request's final status (matched in order by method and path). Prints markdown:
per request class the requests and the facilities they use, then facility totals and T5.
"""
import collections
import json
import sys
import urllib.parse


def shape(method, target):
    path = urllib.parse.urlsplit(target).path
    segs = [s for s in path.split("/") if s]
    out = []
    for i, s in enumerate(segs):
        out.append(s if s.startswith(":") else ("{index}" if i == 0 else "{path}"))
    if len(out) > 2 and not out[-1].startswith(":"):
        out = out[:1] + ["{path}"]
    tail = "/" if path.endswith("/") and segs and not segs[-1].startswith(":") else ""
    return f"{method} /" + "/".join(out) + tail


def load(census, newman):
    lines = [json.loads(l) for l in open(census)]
    reqs = [l for l in lines if l["seq"]]
    outside = [l for l in lines if not l["seq"]]
    if newman:
        ex = json.load(open(newman))["run"]["executions"]
        j = 0
        for e in ex:
            r = e["request"]
            path = "/" + "/".join(urllib.parse.unquote(p) for p in r["url"]["path"])
            while j < len(reqs) and not (reqs[j]["method"] == r["method"]
                                         and urllib.parse.unquote(urllib.parse.urlsplit(reqs[j]["target"]).path).rstrip("/") == path.rstrip("/")):
                j += 1
            if j == len(reqs):
                break
            reqs[j]["http"] = e.get("response", {}).get("code", 0)
            reqs[j]["name"] = e["item"]["name"]
            j += 1
    return reqs, outside


def main():
    allreqs, outside = load(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    reqs = [r for r in allreqs if "http" in r] if len(sys.argv) > 2 else allreqs
    probes = len(allreqs) - len(reqs)
    total = collections.Counter()
    users = collections.defaultdict(set)
    classes = collections.defaultdict(list)
    for r in reqs:
        for k, v in r["counts"].items():
            total[k] += v
            users[k].add(r["seq"])
        classes[shape(r["method"], r["target"])].append(r)
    print(f"requests with a census line: {len(allreqs)}; suite requests (matched to the newman report): {len(reqs)}; "
          f"readiness probes: {probes}; tainted: {sum(r['tainted'] for r in allreqs)}; unattributed lines: {len(outside)}\n")
    print("## Request classes\n")
    print("| Class | Requests | Facilities (requests using each) |")
    print("|---|---|---|")
    skip = ("t5.", "shard.", "write.", "query.nodes", "query.prepared", "query.kind.", "search.calls", "search.shards")
    for cls in sorted(classes):
        rs = classes[cls]
        use = collections.Counter()
        for r in rs:
            for k in r["counts"]:
                use[k] += 1
        keep = sorted(k for k in use if not k.startswith(skip))
        shard = sorted(k[6:] for k in use if k.startswith("shard."))
        cells = ", ".join(f"`{k}` {use[k]}" for k in keep)
        if shard:
            cells = ("shard: " + ", ".join(f"{k} {use['shard.' + k]}" for k in shard) + ("; " if cells else "")) + cells
        print(f"| `{cls}` | {len(rs)} | {cells} |")
    print("\n## Facility totals\n")
    print("| Facility | Requests | Count |")
    print("|---|---|---|")
    for k in sorted(total):
        print(f"| `{k}` | {len(users[k])} | {total[k]} |")
    print("\n## Outside any request\n")
    agg = collections.Counter()
    samples = []
    for o in outside:
        agg.update(o["counts"])
        samples += o["samples"]
    for k in sorted(agg):
        print(f"- `{k}` {agg[k]}")
    print("\nSamples: " + ", ".join(f"`{s}`" for s in samples[:10]))
    print("\n## T5 violation samples (key prefix Q)\n")
    for r in reqs:
        for smp in sorted(set(r["samples"])):
            if smp.startswith("t5[Q].") and "key_term:" not in smp:
                print(f"- {r['seq']} {r['method']} {r['target'][:60]}: `{smp}`")
    print("\nT5 samples with the other candidate prefix `_id.` are in the capture file; none of the suite's documents has such a term.")


main()
