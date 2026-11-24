# field-check-at-map: journal

State: waiting on jobs 261124-145133-73466a 261124-145136-a057fa 261124-145139-a2c903
Quest: .campaign/xapiand/quests/field-check-at-map.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix round 1 (A3 memory, the lead's assignment): both A3 rows at strata:55a5ea73 failed on memory
(df0b09 resident, fcbe11 dirty and resident). Cause, measured with the counting build
(`eager_open_count`, 200 clones, `~/.scratch/xapiand/field-check-at-map`): the same allocations
as main, 16 more bytes per segment on every arm, lazy included. `vouch` grew 16 -> 32 B (table
size, optional key), so `open_segment` 200 -> 216 B (malloc 208 -> 224). Not per field, not the
check: per segment, the record.

Fix at strata:239d51e4: `open_segment`'s `version` and three flags share one word (they padded
three), so the record is 200 B again with the full claim. Heap per index, main `today` / 55a5ea73
`eager` / fixed `eager`: d1000s4 28,489 / 28,553 / 28,489; d1000s10 56,497 / 56,657 / 56,497;
lazy likewise +64/+160 -> 0. Equal byte for byte, after open and after the first query. Rows in
`docs/perf/eager-open.md` "The record's size"; `docs/mechanisms/field-check.md` says it.

Queued once at strata:239d51e4 (pushed): A1 head 261124-145133-73466a; A3 strict rows d1000s4
261124-145136-a057fa, d1000s10 261124-145139-a2c903. Not rerun: A2 identity (no read path
changed: a member order), the A1 mutant (vouch::check unchanged). `campaign verify --for A1,A4`
defers build-test to the round's end (State ready for review).

## Next

1. Read the A3 rows; fill Evidence; set ready for review (build-test then reruns).

## Blocked on

Nothing. File set note for the lead under "Rulings I made" (1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261124-145133-73466a (head, 239d51e4); 261124-120815-138f97 (mutant, --fails-without: passed) |
| A2 | 261124-122659-a4de45 |
| A3 | 261124-145136-a057fa (d1000s4), 261124-145139-a2c903 (d1000s10), at 239d51e4; failed at 55a5ea73: df0b09, fcbe11 |
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

4. A3 fix: repack the record, not shrink the vouch. A 24 B vouch (u32 table size, u32 key with a
   sentinel) would take the record to 192 B, but a "no bound" sentinel either needs a flag or
   skips the check for a 2^32-1-field manifest; the repack keeps the explicit types at main's size.

## For the lead (not this quest's)

- prospect: `fielddict::reader::check_fields` scans the pool through `pool()`, which caches it in
  the reader (map node + `termdict::reader`): 400 B in 2 allocations per segment, kept while the
  index is open, on main too (Xapiand's `verify_fields` path). Scanning through a reader of its
  own (a `transient` helper beside `dictionary()`) measured: `eager` and `today` heap per index
  -1,600 B (d1000s4) and -4,000 B (d1000s10), 7% of a hot index; same identity hash, same
  `d1000s4c` refusals. Patch at `~/.scratch/xapiand/field-check-at-map/src/prospect-fix`
  (fielddict.hh); a prospect quest (prospect-check-fields or its own).

## Messages

- Lead (no id): A1 head job 261124-120813-03d164 was cancelled by me; noted, evidence is now
  queued once at the ready commit and not cancelled.
