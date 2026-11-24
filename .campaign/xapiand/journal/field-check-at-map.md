# field-check-at-map: journal

State: waiting on jobs 261124-120815-138f97 261124-122655-73d9cf 261124-122659-a4de45 261124-122702-df0b09 261124-122705-fcbe11 261124-122723-cb483e
Quest: .campaign/xapiand/quests/field-check-at-map.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 is code-complete at strata:55a5ea73 (pushed). Evidence queued once at that commit:

- A1 head: 261124-122655-73d9cf; A1 mutant (strata:da075624, `mutant/field-check-at-map/skip-check`,
  `vouch::check` skips `check_fields`), `--fails-without`: 261124-120815-138f97. A local run of the
  mutant failed the merge case's checks; the head passed.
- A2 identity (both sides over prospect:4430396, base strata:cfa20b41): 261124-122659-a4de45.
- A3 strict rows after A2, `summary.py --within today:eager`: d1000s4 261124-122702-df0b09,
  d1000s10 261124-122705-fcbe11.
- `campaign verify` build-test (A4): 261124-122723-cb483e. CI runs on the push (A6).

Local smoke (10 indexes, one binary on every side): identity ok; d1000s4c refused at open on
eager arms, at the first read on lazy, with corrupt_segment. The lazy message is wrapped twice
("strata: corrupt segment: strata: corrupt segment: ..."), as on main for any first-map refusal.

## Next

1. Read the jobs; fill Evidence and Result; add the A3 rows to `docs/perf/eager-open.md`'s last
   section (strata) and commit.
2. Then ready for review.

## Blocked on

Nothing. File set note for the lead under "Rulings I made" (1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261124-122655-73d9cf (head), 261124-120815-138f97 (mutant, --fails-without) |
| A2 | 261124-122659-a4de45 |
| A3 | 261124-122702-df0b09 (d1000s4), 261124-122705-fcbe11 (d1000s10) |
| A4 | 261124-122723-cb483e (build-test) and CI |
| A5 | `docs/mechanisms/field-check.md` (strata) |
| A6 | <pending CI> |

## Result

<pending>

## Rulings I made

1. File set: index-restructure split `index.hh`, so the map's check sits in `index_open.hh`
   (`open_body_of`), outside the quest's file set; the quest also lands the explorer's harness
   (`bench/eager_open/**`, A3) and its perf note (`docs/perf/eager-open.md`). I edit those three
   paths; the lead should add them to the file set. `open_body_of` only calls `v.check(seg)`; the
   claim's checks live in `vouch` (index.hh), in one place.
2. The test plugin is strata's own (a stored plugin whose records are field ids, its reader with
   `check_fields`), not prospect's: strata's suite does not depend on prospect. Prospect main
   already forwards `inverted_plugin::reader::check_fields` (prospect:4430396), so the A2/A3 jobs
   exercise the real dictionary check.
3. A3's rule: Xapiand's open after the move (proto `eager`, the check at map, no host loop)
   against today's (base `today`, Xapiand's `verify_fields` loop), per fixture: proto's median open
   p50, open CPU, dirty and resident at most the top of the A/A band (base and aa runs of `today`).
   proto `today` (both checks, until drop-open-verify lands) and base `eager` (no check at all) are
   reported beside it, not judged.

## Messages

- Lead (no id): A1 head job 261124-120813-03d164 was cancelled by me; noted, evidence is now
  queued once at the ready commit and not cancelled.
