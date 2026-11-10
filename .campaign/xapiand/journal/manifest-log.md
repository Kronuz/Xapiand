# manifest-log: journal

State: waiting on jobs 261109-154838-ab0dbc 261109-154840-db382b 261109-154841-35d7ea 261109-154842-e89e93 261109-154844-624428 261109-154845-6a1734 261109-154903-20023c
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fourth contributor. Step 3c at strata:9d4630c: T4 case 1 (lifecycle), T6 against `main`, A5's
writer half (a fix: any failure past the durable boundary fences), the A3 open-read row, the
tools/mutants port (lead's ruling 1). Jobs at strata:9d4630c: build-test 261109-154838-ab0dbc,
mutants 261109-154840-db382b (29 log and index mutants, then `tools/mutants/fields.py`), keys
mutants 261109-154841-35d7ea, A3's T7 row (strict) 261109-154842-e89e93, T6 vs main
261109-154844-624428, both fuzz targets 5 minutes each 261109-154845-6a1734, A9's Xapiand half
261109-154903-20023c (Xapiand master Xapiand:c8276fae3 with `FETCHCONTENT_SOURCE_DIR_STRATA`).
Mutants 261109-143552-5afa2f at strata:4d2a541: 23 of 23 killed.

Locally at strata:9d4630c (macOS, Release): manifest_log, apply_atomic (152 commit allocation
points: 139 retried, 12 fenced after the edit was durable, 1 absorbed), lifecycle's rollover part
(2,000 points over 159 workloads, 159 rollovers at generation 130, 88 cleanups, 38 inside
recoveries, 22.8 s), index, digest_vs_ref (80 streams equal to `main`).

## Next

1. Read the jobs at strata:9d4630c; fix what is red with a rerun of that probe only.
2. A9's prospect half: when manifest-log-prospect's branch has its port committed (its journal:
   pinned at strata:4d2a541, port uncommitted), run `test/prospect_integration.sh` at this head with
   `--ref prospect=/Users/kronuz/code/Kronuz/prospect@<its branch commit> --ref main=main`.
3. Fill the Evidence rows for A2 to A5 and A9 from the jobs; set State "ready for review" once
   every criterion but A8 has a row; ask the lead to push for CI (`linux-asan-ubsan`).

## Blocked on

Nothing. The last Blocked on is answered by the lead's rulings L4 and L5 below.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261109-143551-b9d053 at strata:4d2a541 (72 of 72): manifest_log ("appender": one write per append, the truncate-back, one record in flight), index (commit, recover, `check()`), retention (D8 across a rollover), lifecycle; the retired files' codecs are gone (strata:074129c) |
| A6 | build-test 261109-143551-b9d053 at strata:4d2a541: manifest_log_test stage "caller rules" |
| A7 | build-test 261109-143551-b9d053 at strata:4d2a541: manifest_log_test stages "crash images" and "bit flips", media_test (every byte of the log) |
| A10 | D41 (PLAN.md, 2026-11-09): Q1 the manifest log first, Q2 a count of generations with `fallback()` reporting the damaged generation, Q3 accepted; folded into `docs/design/manifest-log.md` at strata:49ab291 |

Step 3a's checks, which A3, A5, A6 and A7 build on (rows when the index runs on them, step 3c):
`manifest_log_test` stages "caller rules" (A6), "crash images" and "bit flips" (A7), "bounds on
read" and "replay cost" (A3 without the delete cost), "allocation failure" (A5, the open's half),
"appender" (A1's log writer), in the build-test check 261109-112947-1006c3; 19 mutants in
261109-112951-b8f62d (a local run killed 18 of 19; the survivor was a mutant that did not remove
the damage check, rewritten).

## Result

Not ready for review: step 3c's jobs at strata:9d4630c are pending, and A9's prospect half waits
for manifest-log-prospect's port.

Step 3c (strata:1b9e46b to strata:9d4630c): T4 cases 1 and 4 to 9; A5's writer half with a fix
(R27: any failure past the durable boundary fences; 152 commit allocation points, 12 fenced with
the edit durable, 139 retried, 2 failing before the fix); T6 equal to `main` on 80 streams
(verdicts, per-commit digests, reopen, pins; `test/digest_streams.cc`, `test/digest_vs_ref.py`);
T4 case 1 over 2,000 crash points of rollovers and lazy cleanups; A3's open read on the 20M-doc,
10M-deleted shape locally 41.8 ms for the snapshot alone and 39.8 ms with the 128 delete edits
(replay within noise; 8.9 s on ordinal vectors), the edits' apply 0.07 ms. Docs:
`docs/mechanisms/index-on-the-log.md` (the fence rule, the new proofs).

Step 3b, the index on the log (strata:f2089f9 to strata:074129c): commit appends the edit or rolls
over by D7, recover replays and syncs only the log, retention reads kept generations from the log
holding them, `check()` replays the log; the manifest, live-docs, metadata and field-table files
and their codecs retired, `pre_freeze` 0x8000'0008. Measured locally: media_test faults every byte
of the log (669 bytes, 861 offsets in all, 1,722 faults of each kind), every sticky log fault
fails the open, the digest never wrong. Docs: `docs/mechanisms/index-on-the-log.md`; field-table,
manifest-log-codec, segment-records and warm-indexes notes updated for the retired files.

Step 3a, the log layer, at strata:6a8bb32 (index.hh untouched):

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
- R23 (the retirement): `files_test`, `fuzz_files`, `fuzz_fields` and their corpora, fields_test's
  decoder stage and the golden manifest, live-docs and metadata fixtures leave with the codecs;
  `format12/manifest.bin` and `pre-d2/manifest-2` stay as real old manifests that read_log refuses
  as `unsupported_index` (manifest_log_test). `open_probe.cc` is not part of the build and models
  `main`: it is pinned to strata:7ffabab instead of carrying a private copy of the codecs.
  `field_table_bench` reports the final table as snapshot bytes (`decl_bytes`). The defer_io
  default-path hash moves only by the version byte (verdicts, mutations and digest unchanged).
- R24 (T4 mutants by hand): case 4 needs no new mutant (no truncate-back fails the short-write
  case); case 6, retention from the current log alone, fails retention_test; case 7, no snapshot
  in the allowance while a rollover is owed, admits 298 documents instead of 171 and the commit
  fails; case 9, the open without its log sync, loses the edit on 31 of 64 seeds. They are not in
  `manifest_log_mutants.py`, which mutates only the log layer.
- Lead's rulings, 2026-11-09 (the last Blocked on):
  - L4: `tools/mutants/**` joins the file set; delete or port the mutants of the retired codecs.
  - L5: A3's T7 job is a short `strict` job at the head running T7's replay-cost row on the
    20M-doc, 10M-deleted shape (the 128 one-ordinal delete edits); a functional bound. A8's T rows
    stay the lead's on `main` after landing.
- R25 (L4): deleted `fields.py`'s `commit-forgets-the-file` (stale on `main` already),
  `torn-tail-appended-over`, `check-skips-the-file` and the five `decoder-*`, and `keys.py`'s
  `manifest-keeps-a-prefix-field`: their files and codecs are gone. Ported the five that only
  followed `m_` to `cur_` and `id-without-mark-resolves` (stale on `main`: the resolution gained the
  key field). The decoder's name-once and one-key-field rules moved to the log: tests in
  manifest_log_test (a snapshot's table, an edit's own declarations) and three mutants in
  `test/manifest_log_mutants.py`; block CRC and trailing bytes are the framing's and the decoder's
  (bit flips, `snap + '\0'`).
- R26 (T4 case 1's scope): strata has no commit batcher (small-indexes D3 is not built) and faultfs
  is one directory per filesystem, so "3 to 100 indexes with batched commits" has nothing to crash
  yet; it comes with the batcher. The case covers every mutation of each D7 rollover and each old
  log's removal; the truncate-back's two mutations are case 4's (strata:39b051d); two edits in
  flight is the log layer's mutant ("two records in flight"). Measured first: the 150-step
  workloads never rolled over; commit-heavy ones (a commit after every transaction, 200 steps)
  roll over at generation 130 (R = 128) and remove the old log within 90 commits in 88 of 159.
- R27 (A5's writer half, a finding): an allocation failing in the appender's sync left it broken
  and the writer unfenced (every later commit threw "the appender failed", never fenced), and one
  failing after the durable edit outside `apply()` (the head's file names for retention) left the
  head ahead of memory, unfenced; the rollover's ordering point, rename and directory sync had the
  same gap for anything but `std::system_error`. `index.hh`'s `durable()` fences on any exception
  there. apply_atomic_test's commit sweep failed at 2 points before the fix; mutants "the head's
  replay fences only on damage" and "the log's barrier fences only on a system error" hold it.
- R28 (T6 against `main`): `test/digest_streams.cc` uses only API `main` shares; it opens pinned
  checkpoints newest first, since `main`'s `open(..., to)` drops the manifests above `to` (D6.8)
  and the head's keeps the logs. `test/digest_vs_ref.py` compiles it against `git archive REF
  include` with the build's own flags and dependencies.
