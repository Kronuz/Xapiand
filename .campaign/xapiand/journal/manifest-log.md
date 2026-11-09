# manifest-log: journal

State: working
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Third contributor. Step 3b's red tests fixed at strata:cd8156d (R20, R21); the five failures at
strata:7ffabab were this branch's: build-test at strata `main` (strata:3df7af4) 261109-133205-fdea61
passed 74 of 74. Retiring the old codecs (Next 1), uncommitted.

## Next

1. Retire `files::manifest`, the live-docs, metadata and field-table file codecs, `files_test`,
   `fuzz_files`, `fuzz_fields` and their corpora, fields_test's decoder stage, the golden
   fixtures; `pre_freeze` to 0x8000'0008 (D11); regenerate `test/fixtures/manifest_log` and the
   log fuzz seeds; own commit, build-test job.
2. Step 3c: T4's cases on faultfs, media_test on the log (ported by renames only; check its cases
   cover the log), A3's 20M-doc delete row, the fuzz runs at the ready head, A9 (prospect waits for
   manifest-log-prospect).

## Blocked on

Nothing open. (Former 1 resolved by L1; former 2 resolved by the lead's file-set change,
Xapiand:143eddeef: R22.)

## Evidence

| Criterion | Evidence |
|---|---|
| A10 | D41 (PLAN.md, 2026-11-09): Q1 the manifest log first, Q2 a count of generations with `fallback()` reporting the damaged generation, Q3 accepted; folded into `docs/design/manifest-log.md` at strata:49ab291 |

Step 3a's checks, which A3, A5, A6 and A7 build on (rows when the index runs on them, step 3c):
`manifest_log_test` stages "caller rules" (A6), "crash images" and "bit flips" (A7), "bounds on
read" and "replay cost" (A3 without the delete cost), "allocation failure" (A5, the open's half),
"appender" (A1's log writer), in the build-test check 261109-112947-1006c3; 19 mutants in
261109-112951-b8f62d (a local run killed 18 of 19; the survivor was a mutant that did not remove
the damage check, rewritten).

## Result

Not ready for review. Step 3a, the log layer, at strata:6a8bb32 (index.hh untouched):

- `read_log` (D6.2 to D6.4, D6.8): journal's scan, then D6.3's caller rules, replay within twice
  D7's bounds (SR2-strata), `to` with damage past it reported, `log_damage` naming the damaged
  generation; any exception drops the partial state (SR4, the open's half).
- `log_appender` (D4): one write per append in sector-aligned pieces, the truncate-back and its
  sync, one record in flight, a broken state after a failed truncate-back or barrier.
- `apply()`: I13 against an identity set the state keeps, names against the edit's own set.
  `field_table::append` grew its vectors by one per append (quadratic); it grows geometrically.
- Measured locally (macOS, RelWithDebInfo): 2,484 honest crash images over 40 logs, 0 misread (497
  whole, 809 torn, 1,178 previous clean); 35,200 single-bit flips, 0 silent, 40 in padding; ten
  times the declarations in one edit 14.6x the time (118x before the fix); edits adding segments
  onto a hundred times the state 1.6x; 58 allocation points, each bad_alloc, none misreported.
- Design: C4 to C6 and D41 folded in (strata:49ab291).
- Docs: strata `docs/mechanisms/manifest-log-file.md`; `test/manifest_log_mutants.py`;
  `fuzz/fuzz_manifest_log_file.cc` with 48 seeds.

## Rulings I made

- R1 (scope): step 3 does not fit one review: `index.hh`'s commit, recovery, retention, `check()`,
  install and the retired files touch about 160 lines that name the manifest, live-docs, metadata
  and field files, plus about 90 test sites in strata and prospect's tests that read the old files.
  I split it into 3a (the log layer, reviewable alone), 3b (the index on the log) and 3c (the crash
  suite and integration), each its own commits, as the lead's prompt allows.
- R2: merged strata `main` (strata:3df7af4) into the branch rather than rebasing, so the
  reviewed design commits (strata:1c8ff50, strata:79beaf4) the reviews cite stay reachable.
- R3: `field_table::append` reserved size + 1 per append, which libc++ honors exactly, so each
  append copied the table: quadratic in a log replay and in the index's own declarations. It now
  grows geometrically, keeping its strong guarantee (reserve before any change). Shared code inside
  the file set; the "replay cost" stage and a mutant hold it.
- R4: F, R and c are constants, not options; the writer rolls over past 1x D7's bounds and the
  reader refuses past 2x, leaving room for a log written under a larger preset.
- R5 (C5): of the re-check's two fixes I took the mechanism ones: the replay bound scales with S
  (max(R, n, S / c), c = 512) and the allowance holds S only while a rollover is forced, D7's
  bounds waiting one commit under ENOSPC. Written into D7 and D8.
- R6: `open(..., to)` reads to `to` and reports damage after it instead of failing, so a damaged
  log still opens at a whole generation before the damage (D41's Q2); without `to` damage is
  `log_damage` with the generation.
- R7: `log_state` keeps its segments' identities as a derived set (rebuilt by `recount`), so I13
  costs the edit; equality stays a function of the segments.
- R8: the sanitizer fuzz runs (both log targets, 5 minutes each) wait for the head marked ready,
  as the prompt asks; the replay of their corpora runs in every build.
- Lead's rulings, 2026-11-09:
  - L1: Blocked on 2 yes: the lead opens a prospect companion quest after manifest-log that repins
    strata and moves prospect's tests to the log (the list under Blocked on 2 is its scope); A9's
    jobs run prospect at its branch. This quest does not edit prospect.
  - L2: step 3a gets no separate review; one durability and one security review at the head marked
    ready. The decoder's fuzz targets (`fuzz_manifest_log`, `fuzz_manifest_log_file`) stay ready
    for the security reviewer.
  - L3: Blocked on 1 is with the owner (the lead recommended option 1). Step 3b continues on
    everything that does not touch the deleted set; the deleted-set change lands in its own commit
    when the answer arrives.
- Owner's ruling D42 (PLAN.md, 2026-11-09), formerly Blocked on 1 (SR3's deleted-set
  representation, A4 and A3's delete cost): option 1, an adaptive deleted set, bitset words when
  deleted >= doc_count / 64 and sorted ordinals otherwise; a bitset record decodes to words, a
  deltas record to ordinals (memory at most 8x the record); the index converts to `deleted_bits`
  at open after the size check.
- R9 (D42's cost): a delete edit's walk to each new ordinal's old neighbours (for the delta bytes
  S needs exactly, C7) covers disjoint stretches, so it costs O(k log n) on ordinals and at most
  the set's words once on words, the same order D42 accepted for a merge into ordinals.
- R10 (change list): strata has no commit of a past generation yet (durability-modes D1's
  checkpoint is K2's), so the change list holds the changes since the head without marks; the
  edit is always head to current state. Marks come with the first caller that commits a past
  target. T6's past-target cases stay at the log layer (diff/apply round trips).
- R11 (head memory): the writer keeps the head as a full `log_state`: its deleted sets (at most
  the segments' bits again, by D42's forms), metadata map and field table duplicate the index's.
  Sharing the table's prefix and the metadata is a follow-up if T9 or a heap row shows it.
- R12 (retention): a predecessor generation whose log ends before it (a generation a torn tail
  skipped, D6.7) names nothing and does not block; a pin that cannot be read, or any other read
  failure, blocks the round as `main` does.
- R13 (`fallback()`): with the log, `newest` is the newest generation the logs hold whole or the
  first damaged one (D41's Q2: "damage at generation x"), and `discarded = newest - to`, counting a
  pinned checkpoint's gap; retention_test checks both. The fallback's rollover starts above
  `newest`, so its log never reuses a generation another log holds (index_test's recover --to).
- R14 (retention at 0): the predecessors are edits of the head's log, so `open(..., to)` at a
  predecessor reads it whole while its files are on disk, at any `retained_predecessors`; D8
  promises nothing for an obsolete one. retention_test asserts the open at 0 and 1 alike.
- R15 (D6.6): the open runs no retention; it is owed to the first commit, `reclaim()` or a drain
  (admission's, or `reclaim_for_space()` under defer_io, which never reads logs inside apply).
  Tests that saw a queue or a blocked round right after an open now see it after that step.
- R16 (faultfs): an ordering point staged bytes and names only ahead of later namespace changes;
  a later file sync (the log's append) did not make them durable, so faultfs let the log's record
  survive without the segment it names. The barrier's second phase is now modelled: a file sync,
  or one that fails after its effect, makes everything staged before it durable, as
  `F_FULLFSYNC` after `F_BARRIERFSYNC` does (Linux's ordering point already fsyncs). Two
  faultfs_test cases fail without it.
- R17 (goldens and counts): defer_io's default-path golden is re-captured (one apply more meets
  the 150 KiB budget, a log's edits holding more bytes than one rewritten manifest; 117
  mutations, not 163); warm's crash workload is 32 mutations (bound 25, was 40); a create makes 1
  file sync and 2 directory syncs (was 2 and 2); an index whose only file is its log reads as never
  created once the log is gone (lifecycle).
- R18 (the allowance): a demoted index's closed log descriptor does not owe a rollover, so the
  allowance holds a snapshot only while `rollover_owed_` (warm's turn-bound drain caught it);
  the open settles the storage permit (index_test's permit check caught it missing).
- R19 (tests removed): the index tests that crafted live-docs or metadata files (another
  generation's, another index's, a wrapping bitset or delta) have no file left to craft; the
  decoder's own bounds are manifest_log_test's "bounds on read", "bit flips" and the fuzz target.
  The install and open damage cases are re-made as resealed snapshots.
- R20 (seeded identities): a seeded index restarted its identity count at every open, so a reopen
  drew a segment identity its log already held and the log refused the edit (apply_atomic aborted;
  `main` never checked I13 on write). The seeded base is the seed plus 2^32 per generation
  committed before the open, set after recovery; index_test's seeded-reopen case fails without it.
- R21 (D6.6 in tests): background_merge and reclaim_bg expected the open to queue its orphans; under
  D6.6 that is owed to `reclaim()` or the drain, so they call it or expect it there. reclaim_bg's
  threaded case compared the permit with a fresh count while a rolled-over log's removal (91 KB
  measured) awaited its directory sync, failing 1 run in about 20; it drains first (200 runs clean).
- R22 (former Blocked on 2): the lead added `bench/field_table/**` to the file set
  (Xapiand:143eddeef); strata:7ffabab stays.
