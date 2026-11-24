# field-check-at-map: journal

State: working
Quest: .campaign/xapiand/quests/field-check-at-map.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 code written in strata worktree `strata-wt/xapiand/field-check-at-map`: vouch carries the
field table's size and key field, `vouch::check` runs at every map; `basic_segment::check_fields`;
`test/field_check_map_test.cc`; `bench/eager_open` (the explorer's harness, both sides over one
prospect); docs. Local build of the test queued under `quiet light` (host load 86-144).

## Next

1. Local build + run of field_check_map_test; fix; commit.
2. Mutant commit (skip the field check in `vouch::check`) and A1 `--fails-without` job.
3. A2 identity job and A3 strict row job (`bench/eager_open/ab.sh`, `--ref BASE=strata@main`,
   `--ref PROSPECT=prospect@4430396`); `campaign verify field-check-at-map`.

## Blocked on

Nothing. File set note for the lead under "Rulings I made" (1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending> |
| A2 | <pending> |
| A3 | <pending> |
| A4 | <pending> |
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
