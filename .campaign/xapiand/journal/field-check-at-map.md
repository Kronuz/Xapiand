# field-check-at-map: journal

State: waiting on jobs 261124-185647-29ad76 261124-185649-bbc152 (A3 rows at prospect:c99e6be)
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

## Fix round 2 (lead: d1000s10 still over at 239d51e4)

At strata:239d51e4: A1 73466a and d1000s4 a057fa pass; d1000s10 a2c903 fails, dirty 64,295 B
over 64,147, resident 228,483 over 228,336; the proto build's `today` arm is high too.

Measured (`~/.scratch/xapiand/field-check-at-map`, 1,000 indexes, d1000s10):
- Heap is equal: malloc zone stats, blocks and bytes in use per index, base `today` 61,774-61,779,
  proto `eager` 61,775-61,777 (so no size-class or uncounted allocation).
- vmmap -summary of one paused run each: same 349,188 allocations, 58.9 MB allocated; frag
  2,383 KB (base today) vs 2,539 KB (proto eager), +156 B/index, MALLOC_TINY dirty 14.7 -> 14.8 MB.
- Cause: the map runs prospect's check mid-load, and `fielddict::reader::check_fields` opens the
  pool into the reader's cache (400 B, 2 allocations per segment, kept). Xapiand's loop opened
  the same cache after each open; mid-load it lands among the load's short-lived allocations.
- Experiment, closing the segment file before the check: proto eager 64,287-64,295 vs base today
  64,086-64,119 (from 64,332-64,369): under 50 B. Reverted.
- Prospect's check through a reader of its own (patch: `src/prospect-fix`, fielddict.hh): base
  `today` 59,859-59,961 (median 59,890), proto `eager` 59,855-60,035 (median 59,937), 6 runs
  each; both 4.2 KB/index under today's 64.1 KB. Same identity hash, same d1000s4c refusals.

Docs: strata:6dc38c77 (`docs/perf/eager-open.md` "The record's size", field-check.md), pushed.
Nothing queued: the fix is outside the file set.

## Resume (prospect-pool-reader-transient landed at prospect:c99e6be)

Queued strict A3 rows at strata:6dc38c77 (docs-only over 239d51e4), BASE strata:cfa20b4 as before, PROSPECT prospect:c99e6be, same commands as a057fa/a2c903: d1000s4 261124-185647-29ad76, d1000s10 261124-185649-bbc152. No code change, so A1 (73466a) stands and the A1 mutant is not rerun. Then: write the rows into docs/perf/eager-open.md, `campaign verify field-check-at-map`, ready for review. Note: `campaign brief` says main changed 12 files since the branch point; the lead merges main (or tells me to).

## Next

1. On the lead's ruling: option 1 -> rerun A3 d1000s10 (and d1000s4) with PROSPECT at the
   prospect commit; option 2 -> lifecycle/site note, then code.

## Blocked on

Answered (lead, 2026-11-24): option 1. The prospect change is its own quest, prospect-pool-reader-transient (started, contributor running); this quest is ordered after it (`after`) and parked until it lands. Then a resume reruns both A3 rows with PROSPECT at the landed prospect commit (strict, same commands as a057fa and a2c903), sets ready for review, and build-test reruns.

The lead: the remaining d1000s10 gap is placement of prospect's cached pool reader, not strata's.
1. (Recommended) Prospect: `check_fields` scans the pool through a reader of its own (patch ready,
   ~10 lines, measured above). Land it (prospect-check-fields or a new prospect quest, or add
   prospect `include/prospect/fielddict.hh` to this file set), then rerun A3 with PROSPECT at it.
   Removes 4.2 KB/index on every arm; the d1000s10 gap falls within the run spread (+47 B median
   on 6 local runs; the strict row decides).
2. Strata only: run the eager open's checks after the load loop, as Xapiand's loop did. Splits
   the one check site in two and keeps the 4 KB cache; not recommended.
File set note for the lead under "Rulings I made" (1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261124-145133-73466a (head, 239d51e4); 261124-120815-138f97 (mutant, --fails-without: passed) |
| A2 | 261124-122659-a4de45 |
| A3 | d1000s4 261124-145136-a057fa (passed, 239d51e4); d1000s10 261124-145139-a2c903 failed (see Fix round 2); failed at 55a5ea73: df0b09, fcbe11 |
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

- Lead (no id, d1000s10 still over at 239d51e4): measured; cause is prospect's pool-reader cache
  opened mid-load (fragmentation, vmmap); fix is prospect's; blocked on the lead's choice.
- Lead (no id): A1 head job 261124-120813-03d164 was cancelled by me; noted, evidence is now
  queued once at the ready commit and not cancelled.
