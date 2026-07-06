#!/usr/bin/env python3
# Assertion-only e2e gate -- the docs ARE the spec.
#
# Every request doc carries its own `pm.test(...)` assertions (in a {% comment %}
# block, so they never render), and docs_to_postman.py synthesises a default for any
# request that lacks one: `pm.response.to.be.success` (2xx), or the specific status
# pinned by a `status: NNN` marker for the deliberate error demos. So every request
# is verified, and the whole expected result lives next to the request in the .md.
#
# GREEN iff EVERY documented assertion passes and EVERY request is covered. No
# response-body baseline, no allowlist, no skipped tests: if a documented assertion
# fails, the docs and the server disagree and that is a red build (fix the code, or
# fix the doc's expectation). This runs anywhere -- including CI -- straight from the
# docs, with nothing to capture, distill, drift, or store per machine.
#
# Usage: e2e_assert.py REPORT.json
import json
import sys

execs = json.load(open(sys.argv[1]))["run"]["executions"]

failing = []
uncovered = []
n_assert = n_pass = 0
for e in execs:
    name = (e.get("item") or {}).get("name", "?")
    a = e.get("assertions") or []
    if not a:
        uncovered.append(name)
        continue
    for x in a:
        n_assert += 1
        if x.get("error"):
            msg = (x.get("error") or {}).get("message", "")
            failing.append((name, x.get("assertion", "?"), msg))
        else:
            n_pass += 1

print(f"requests={len(execs)}  assertions={n_assert} (pass {n_pass}, fail {len(failing)})  "
      f"uncovered={len(uncovered)}\n")

if failing:
    print(f"FAILING assertions (docs vs server disagree): {len(failing)}")
    for name, a, msg in failing:
        print(f"   \u2717 {name}  ::  {a}")
        if msg:
            print(f"       {msg[:200]}")
if uncovered:
    print(f"\nUNCOVERED requests -- no assertion, generator default missing ({len(uncovered)}):")
    for name in uncovered[:25]:
        print(f"   \u00b7 {name}")

ok = not failing and not uncovered
print("\n=== VERDICT ===")
print("GREEN: every documented assertion passed and every request is covered." if ok
      else "NOT GREEN: the docs and the server disagree (see above).")
sys.exit(0 if ok else 1)
