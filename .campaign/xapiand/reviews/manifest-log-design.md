# manifest-log review: design

Commit: strata:79beaf4b52422ef0c1fed83bd982df15d87d188d
Verdict: APPROVE
Date: 2026-11-04

Adopted from the architect's review of the design, written before this campaign tracked the quest:
strata `docs/reviews/manifest-log-design.md` on `design/manifest-log` at strata:79beaf4, section
"Re-check", verdict "APPROVE WITH CONDITIONS: first step 3 may build on revision 1; L1 to L3 are
closed, and three new findings fold in as conditions before step 3 lands, ML-R15 before its recovery
code is written". New blocking items: none. The lead copied the verdict and conditions here so the
campaign's design lens names the reviewed commit; the review's text and probes are in strata.

## Evidence I relied on

The architect's probes in that review: `tail_model.py`, `stale_wrong.py`, `open_probe.cc`,
`rollover_model.py`, `tail_probe_r1.py`, `clean_sync_probe.c` (4,444 crash images, 0 misread;
307,752 single-bit flips, 0 silent).

## What I checked myself

As the review records, under "Re-check".

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1-C3 | no | gap | The preliminary review's conditions stand | review "Conditions" | before step 3 lands |
| C4 | no | gap | ML-R15: the open reports a head that may not be durable after a process crash | review "New findings" | D6.6 picks an option before step 3's recovery code; T4 adds the kill between `pwrite` and `sync_data`, reopen, power loss, with its mutant |
| C5 | no | gap | ML-R5, ML-R16: D8's allowance and D7's replay bound write a whole snapshot per R edits, unbounded on large deleted sets | review "New findings" | D7's bound scales with S (or its sentence is corrected); T5 reports bytes per commit beside `main`'s |
| C6 | no | gap | ML-R6, ML-R17: T8's report and the silent class (lost writes of the log's tail) | review "New findings" | T8 reports all bytes beside `main`'s; Limits and T4 case 3 state the class as ML-R17 words it |

## Not run

As the review records.
