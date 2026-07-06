#!/usr/bin/env python3
# Compare a newman JSON report against an e2e baseline, matching requests by a
# STABLE key (the doc-derived item name + an occurrence counter) instead of by
# position. Positional alignment was the source of the recurring "stale baseline"
# pain: adding or removing one request shifted every request after it, so the whole
# tail showed as spurious diffs. With key matching, an added/removed request is
# reported as exactly that and never cascades; only requests present in BOTH are
# compared.
#
# The baseline is a DISTILLED file (a few KB: per request the failed-assertion set
# + status code, no response bodies), so it is small enough to COMMIT and review in
# a git diff -- unlike the 60 MB raw newman reports, which are gitignored and drift
# per machine. Distill one with:  e2e_diff.py --distill full_report.json > baseline.json
#
# Usage:
#   e2e_diff.py --distill REPORT.json > baseline.json     # make/refresh a baseline
#   e2e_diff.py OURS.json BASELINE.json                   # compare (baseline may be
#                                                          #   distilled or a raw report)
import json
import sys
from collections import defaultdict


def keyed_from_report(execs):
    out = {}
    seen = defaultdict(int)
    for e in execs:
        item = e.get("item") or {}
        name = item.get("name") or "?"
        method = (e.get("request") or {}).get("method", "?")
        url = (e.get("request") or {}).get("url") or {}
        p = "/" + "/".join(url.get("path", []) or []) if isinstance(url, dict) else str(url)
        seen[name] += 1
        code = (e.get("response") or {}).get("code")
        failed = sorted({a.get("assertion", "?") for a in (e.get("assertions") or []) if a.get("error")})
        out[(name, seen[name])] = {"label": f"{method} {p}", "name": name,
                                   "occ": seen[name], "code": code, "failed": failed}
    return out


def load(path):
    """Return {(name, occ): {label, code, failed}} from a raw newman report OR a
    distilled baseline (auto-detected)."""
    doc = json.load(open(path))
    if isinstance(doc, dict) and "e2e_baseline" in doc:          # distilled
        return {(e["name"], e["occ"]): e for e in doc["e2e_baseline"]}
    execs = doc["run"]["executions"]                             # raw newman report
    return keyed_from_report(execs)


def distill(report_path):
    keyed = load(report_path)
    entries = sorted(keyed.values(), key=lambda e: (e["name"], e["occ"]))
    json.dump({"e2e_baseline": entries}, sys.stdout, indent=1, sort_keys=True)
    sys.stdout.write("\n")


if len(sys.argv) == 3 and sys.argv[1] == "--distill":
    distill(sys.argv[2])
    sys.exit(0)

ours = load(sys.argv[1])
master = load(sys.argv[2])
common = ours.keys() & master.keys()
added = ours.keys() - master.keys()
removed = master.keys() - ours.keys()
print(f"ours={len(ours)} baseline={len(master)} requests; {len(common)} matched by name, "
      f"{len(added)} added, {len(removed)} removed\n")

regressions, fixes, preexisting, code_diffs = [], [], [], []
for key in common:
    o, m = ours[key], master[key]
    of, mf = set(o["failed"]), set(m["failed"])
    if o["code"] != m["code"]:
        code_diffs.append((o["label"], m["code"], o["code"]))
    regressions += [(o["label"], a) for a in of - mf]
    fixes += [(o["label"], a) for a in mf - of]
    preexisting += [(o["label"], a) for a in of & mf]

print(f"REGRESSIONS (assertion fails only on OURS): {len(regressions)}")
for label, a in regressions:
    print(f"   \u2717 {label}  ::  {a}")
print(f"\nFIXES (fails only on BASELINE, pass on ours): {len(fixes)}")
for label, a in fixes[:20]:
    print(f"   \u2713 {label}  ::  {a}")
print(f"\nPRE-EXISTING (fail in BOTH, not our regression): {len(preexisting)}")
if added:
    print(f"\nADDED (only in ours -- not compared, not a regression): {len(added)}")
    for k in sorted(added)[:20]:
        print(f"   + {ours[k]['label']}  ({k[0]})")
if removed:
    print(f"\nREMOVED (only in baseline): {len(removed)}")
    for k in sorted(removed)[:20]:
        print(f"   - {master[k]['label']}  ({k[0]})")
print(f"\nHTTP status differences (matched requests only): {len(code_diffs)}")
for label, mc, oc in code_diffs[:40]:
    print(f"   {label}   baseline={mc} ours={oc}")

print("\n=== VERDICT ===")
if not regressions and not code_diffs:
    print("GREEN: no new failures and no status differences on matched requests.")
elif not regressions:
    print("GREEN on assertions (no new failures); status differs on some matched requests (see above).")
else:
    print(f"NOT GREEN: {len(regressions)} assertion(s) regressed vs baseline.")
