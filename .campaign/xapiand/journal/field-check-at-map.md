# field-check-at-map: journal

State: ready for review (jobs running: A3 aab22b a00a3e, A2 d9a59d, mutant 7f00f1, build-test)
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

Queued strict A3 rows at strata:6dc38c77 (docs-only over 239d51e4), BASE strata:cfa20b4 as before, PROSPECT prospect:c99e6be, same commands as a057fa/a2c903: d1000s4 261124-190110-6a800d, d1000s10 261124-190112-00d680. No code change, so A1 (73466a) stands and the A1 mutant is not rerun. Then: write the rows into docs/perf/eager-open.md, `campaign verify field-check-at-map`, ready for review. Note: `campaign brief` says main changed 12 files since the branch point; the lead merges main (or tells me to).

Update: main was merged into the branch (strata:aaf38cd2, catalog split only; no field-check or harness path changed), which cancelled my two jobs (185647, 185649). Already queued by someone else at aaf38cd2 (not mine): A3 rows d1000s4 261124-190110-6a800d and d1000s10 261124-190112-00d680 with PROSPECT prospect:c99e6be and BASE strata main 49fca948 (the merge's main parent, a fairer base than cfa20b4). I kept those and cancelled my duplicates. I queued A1 at head 261124-190113-7d240b and A2 identity at prospect c99e6be 261124-190127-e6a947 (BASE cfa20b4). Then build-test via `campaign verify` at the end.

Lead msg (d1000s10 190112-00d680 unchanged by the prospect fix): cause found, harness not at fault. 190110-6a800d and 190112-00d680 were submitted without `--build`, so they ran the slot's stale `.scratch/eo` binaries; the row.log header says `sides: base cfa20b4 prospect 4430396; proto strata 239d51e4 prospect 4430396`, so neither side had c99e6be (ab.sh passes FETCHCONTENT_SOURCE_DIR_PROSPECT to both sides' builds; the bench CMakeLists pin is only the fallback). 190110's pass is void as evidence too. Resubmitted with the build, BASE strata main 49fca948, PROSPECT c99e6be: d1000s4 261124-191203-86c495, d1000s10 261124-191208-75ce14. Their row.log header must name prospect c99e6be on every side; the today arm should drop ~4 KB/index.

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

Head strata:093bb965 (docs over strata:aaf38cd2, the merge of main). Real builds, BASE strata main 49fca948, PROSPECT prospect:c99e6be.

| Criterion | Evidence |
|---|---|
| A1 | field_check_map_test 261124-205541-baf5c2 and the F1 guard test 261124-205543-2d1c17 (strata:0d39c221; docs-only since); mutants killed: 261124-120815-138f97 (vouch::check), 261124-205545-8c5e65 (F1 guard), 261124-205555-96bca6, 261124-205557-765a8d, 261124-205601-16b399, 261124-211943-7f00f1 (install load) |
| A2 | 261124-211954-d9a59d (identity, rebuilt at prospect c99e6be) |
| A3 | d1000s4 261124-211953-aab22b, d1000s10 261124-211954-a00a3e: all four measures (open p50, open CPU, dirty, resident) of proto eager inside the base/A/A `today` band |
| A4 | build-test 261124-212008-884d7a (queued by verify at strata:093bb965) and CI |
| A5 | `docs/mechanisms/field-check.md`, `docs/perf/eager-open.md` (strata) |
| A6 | <pending CI> |

Void, not evidence: A3 passes before prospect c99e6be and the rows 261124-190110-6a800d / 190112-00d680 (no build step, stale binaries naming prospect:4430396); A2 261124-190127-e6a947 (no build step); the earlier fails df0b09, fcbe11, a2c903.

## Result

The map checks a segment's field ids wherever it maps (`vouch` in `index.hh`, called from `open_body_of`). Eager open with the check at the map costs what today's open costs (Xapiand's `verify_fields` loop): d1000s10 dirty 59,867 B against 59,896 B (aa 59,892), resident 224,051 against 224,088; open p50 2,225 us against 2,174 (aa 2,080), CPU 2,248 against 2,184 (aa 2,095), inside the spread; d1000s4 likewise (dirty 30,147 against 30,167). The record stays 200 B (strata:239d51e4); the remaining gap was prospect's cached pool reader (prospect:c99e6be, its own quest), which also takes 4.2 KB per index off `today` at d1000s10. Rows and method: `docs/perf/eager-open.md` "The A3 rows over prospect's transient pool reader"; mechanism: `docs/mechanisms/field-check.md`.

## Review fixes (correctness review, CHANGES: F1 blocking; strata:0d39c221, pushed)

| Finding | Fix | Confirming job | Fails without |
|---|---|---|---|
| F1 harness runs leftover builds | `ab.sh` `guard` in row, heap, identity: prints `sides:` (identity too), exits 3 when proto's strata != this tree, base/aa != $JOB_REF_BASE, or a prospect != $JOB_REF_PROSPECT (sides' prospects agree when unset); `bench/eager_open/test_guard.sh` stamps matching and mismatched fake builds; method sentence in eager-open.md | 261124-205543-2d1c17 | mutant (ab.sh without the guard) 261124-205545-8c5e65 |
| N1 double "corrupt segment:" prefix | `open_segment::fail(what, rewrap)`: a `corrupt_segment` is rethrown as it is; test asserts one prefix at the lazy first read | 261124-205541-baf5c2 | mutant n1 261124-205601-16b399 |
| N3 wake/heal/install untested | `field_check_map_test`: wake after demote, heal, install (eager and lazy target) from the reviewer's P1-P3 | 261124-205541-baf5c2 | mutants twin (claim dropped on twin) 261124-205555-96bca6, heal 261124-205557-765a8d, install (both records) 261124-205559-b94bac |
| N6 hashes | eager-open.md cites 58f381's identity hashes, the rows' query hashes named as such | docs | n/a |
| reviewer probes | rerun at the head | 261124-205609-44e9fc | n/a |

Results at strata:0d39c221 (lead): A1 baf5c2, guard 2d1c17 (mutant 8c5e65 killed), mutants twin 96bca6, heal 765a8d, n1 16b399 killed, probes 44e9fc passed. Install mutant b94bac SURVIVED, and rightly: it dropped the claim on the install's own copies (`index_checkpoint.hh:59,68`), which hold the bytes `load_files` just checked (manifest identity and footer crc bind them); the field ids there are unobservable except against a payload forged past the stored checksum (tried: a changed id is "stored checksum", not the field check). The check an install rests on is `load_files(source, theirs, whole=true)` (`index_open.hh:449`): mutant install-load (whole skips the claim) fails the test locally (rc 134) and is queued as 261124-211943-7f00f1 at 571f1d7f. No code change; docs-only strata:539fa10e (field-check.md says which check the install rests on and that the pool reader is no longer cached). Brief listed A2/A3/A4 for rerun at the new head (the harness and engine changes): queued once: A3 aab22b (d1000s4), a00a3e (d1000s10), A2 d9a59d, build-test via verify.

Class swept (F1): the three subcommands that run binaries from OUT (row, heap, identity) share `guard`; `test_guard.sh` runs each against each mismatch (stale proto, stale base, other prospect) and the matching case. Left to the hunt list: N2 (prospect naming the field), N4, N5. The A3 rows and A2 stand (harness and test only; their jobs built and printed their sides). Mutant commits are dangling local objects (`~/.scratch/xapiand/field-check-at-map/mut`).

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

- (lead, 2026-11-24) Merged strata main (catalog-restructure) into the branch: strata:aaf38cd2. A3 rows resubmitted there with BASE at strata main 49fca948 (like for like) and PROSPECT c99e6be: d1000s4 261124-190110-6a800d, d1000s10 261124-190112-00d680; A1 rerun at aaf38cd2; build-test at the round's end.

- (lead, 2026-11-24) A2 identity 261124-190127-e6a947 ran without a build step (no sides line; slot binaries), so it is void like the stale A3 rows; resubmitted with the build: 261124-193029-58f381 (BASE strata 49fca948, PROSPECT c99e6be). Cite that one for A2.
