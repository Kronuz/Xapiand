#!/usr/bin/env python3
# Diff two newman JSON reports of the SAME collection (same order) to classify
# assertion outcomes: regressions (fail only in OURS), fixes (fail only in
# MASTER), and pre-existing (fail in both). Usage: e2e_diff.py ours.json master.json
import json
import sys


def load(path):
    execs = json.load(open(path))["run"]["executions"]
    out = []
    for e in execs:
        method = (e.get("request") or {}).get("method", "?")
        url = (e.get("request") or {}).get("url") or {}
        if isinstance(url, dict):
            path = "/" + "/".join(url.get("path", []) or [])
        else:
            path = str(url)
        code = (e.get("response") or {}).get("code")
        failed = set()
        for a in e.get("assertions", []) or []:
            if a.get("error"):
                failed.add(a.get("assertion", "?"))
        out.append({"label": f"{method} {path}", "code": code, "failed": failed})
    return out


ours = load(sys.argv[1])
master = load(sys.argv[2])
n = min(len(ours), len(master))
print(f"ours={len(ours)} master={len(master)} executions; comparing {n}\n")

regressions = []   # failing only in ours
fixes = []         # failing only in master
preexisting = []   # failing in both
code_diffs = []    # different HTTP status

for i in range(n):
    o, m = ours[i], master[i]
    if o["code"] != m["code"]:
        code_diffs.append((o["label"], m["code"], o["code"]))
    only_ours = o["failed"] - m["failed"]
    only_master = m["failed"] - o["failed"]
    both = o["failed"] & m["failed"]
    for a in only_ours:
        regressions.append((o["label"], a))
    for a in only_master:
        fixes.append((o["label"], a))
    for a in both:
        preexisting.append((o["label"], a))

print(f"REGRESSIONS (assertion fails only on OURS): {len(regressions)}")
for label, a in regressions:
    print(f"   ✗ {label}  ::  {a}")
print(f"\nFIXES (fails only on MASTER, pass on ours): {len(fixes)}")
for label, a in fixes[:20]:
    print(f"   ✓ {label}  ::  {a}")
print(f"\nPRE-EXISTING (fail in BOTH, not our regression): {len(preexisting)}")
print(f"\nHTTP status differences: {len(code_diffs)}")
for label, mc, oc in code_diffs[:40]:
    print(f"   {label}   master={mc} ours={oc}")

print("\n=== VERDICT ===")
if not regressions and not code_diffs:
    print("GREEN: no new failures and no status differences vs origin/master.")
elif not regressions:
    print("GREEN on assertions (no new failures); status differs on some requests (see above).")
else:
    print(f"NOT GREEN: {len(regressions)} assertion(s) regressed vs master.")
