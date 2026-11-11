# manifest-log: journal

State: waiting on jobs 261110-114552-f6a3b2 261110-114555-188cd2 261110-114557-207ddc 261110-114559-823541 261110-114602-96a9cd 261110-114604-0667d8 261110-114607-d6584b 261110-114609-102757
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix round 2: durability re-check F3 (blocking; the lead ruled it in scope since R39 routes it
through `durable()`) at strata:f39abc3, and N2 at strata:1ebeff8 (R40, R41). Jobs at the head
strata:1ebeff8, by finding:

| Finding | Job | Expect |
|---|---|---|
| F3 | 261110-114551-711f81: apply_atomic_test (held-view sweeps) and hazard_test, ASan+UBSan, assertions on | done, exit 0: commit 153 points (20 fenced, 17 durable), with a held view 161 (28, 25), reclaiming a merge 150 (20, 17), install 98 (17, 16), install with a held view 109 (28, 27), each alone and with the next failing; hazard ok; no report |
| F3 | 261110-114555-188cd2: the reviewer's `run.sh rel` (alloc_crash_probe, sync_probe) | 0 BAD |
| F3 | 261110-114557-207ddc: the reviewer's `run.sh asan` (both probes, apply_atomic) | 0 BAD, no report |
| F3 mutant | 261110-114602-96a9cd (ASan), 261110-114604-0667d8 (Release): the fix reverted on the head, strata:3b672c4 (`refs/scratch/manifest-log/f3-reverted`) | fail: "the held view reads wrong" or heap-use-after-free |
| N2 | 261110-114559-823541: the reviewer's `mutants.py` at the head | 4 of 4 killed (M2, M3 survived at strata:6ab3983) |
| N2 mutant M2 | 261110-114607-d6584b: install's post-log steps outside `durable()`, strata:810fd13 (`refs/scratch/manifest-log/n2-install-tail`) | fail |
| N2 mutant M3 | 261110-114609-102757: `reclaim_files` alone outside `durable()`, strata:4bb2e08 (`refs/scratch/manifest-log/n2-reclaim-outside`) | fail |
| F3, first cut | at strata:f39abc3 (commit sweep with a held view only): 261110-114306-bd857c exit 0; mutant strata:ca87722 (fix reverted) 261110-114307-f4e492 exit 134, heap-use-after-free at apply_atomic_test.cc:201 (the held view's read), freed by `publish_view` (index.hh:3036) in `commit()`'s `durable` block | as expected |
| all | build-test 261110-114552-f6a3b2 (`campaign verify`) | ctest green |

Fix round 1 is in (strata:c78f870 to strata:08a11ab, one commit per finding; table below). Its
jobs at strata:08a11ab all passed but the mutants job, 36 of 37 killed: "a durability fence set
only once its message is allocated (A5)" survived. Not equivalent (R37): strata:8b532c8 tightens
apply_atomic's commit sweep so an unfenced failure must leave every manifest log byte-identical.
The new check failed at strata:8b532c8 (build-test 261110-052344-8c7d40, CI config
261110-052416-ba4935: "unfenced with its log changed" at the commit's last 7 allocation points,
both bursts): commit() ran publication, retention and reclaim outside the fence rule. Fixed at
strata:6ab3983 (R39). Every evidence job reruns there (the jobs in State).

| Finding | Cause | Fix | Commit |
|---|---|---|---|
| durability F1 | the open synced only the log; a rollover stopped between rename and directory sync kept a head a power loss reverts, and later no-file commits rode on it | the open syncs the directory, then the log (D6.6); T4 case 10 in index_test; mutant "the open syncs the log only" | strata:c78f870 |
| durability F2 | a failed sync_data fenced but left the edit's bytes (Linux keeps them clean in cache) | the appender truncates back to E and syncs before breaking; faultfs `fault::keep` models the clean-but-not-durable page; T4 case 11; mutant "a failed barrier not truncated back" | strata:f9fdcca |
| security F1 | a delete edit walked the dense words to a far neighbour or copied the sparse ordinals: replay O(S^2/c) | dense words carry a summary; sparse ordinals are runs each more than twice the next; manifest_log_test reads logs at 2x D7's edit bound within 4x the snapshot; three mutants | strata:77e5f0c |
| security F2 = fable F1 | an unforced rollover's ENOSPC appended and owed nothing, so sustained ENOSPC grew the log past the reader's 2x bound | the ENOSPC rollover owes the rollover, and the waiting edit appends only within `log_within(..., 2)`; index_test's sustained case; mutant "an unforced rollover's ENOSPC owes nothing" | strata:13d87b2 |
| fable F2 (non-blocking) | append_edit's broken-appender fence built its message before setting `fenced_` | `fenced_` first, as R32 | strata:9915cec |
| security F3 (non-blocking) | the file fuzz target never read with `to` | the input's last byte picks a `to` | strata:08a11ab |
| mutant survivor (A5) | the sweep accepted an unfenced failure whenever the retry reached the twin's digest | an unfenced failure must leave the logs unchanged (R37) | strata:8b532c8 |
| post-commit failures (A5, found by R37's check) | publish_point, free_views, retain and reclaim_files ran after the log moved, unfenced | they run under `durable()`, in commit() and install (R39) | strata:6ab3983 |

Not mine, in no journal: 261110-022840-bb16af, 261110-023641-b98c95, 261110-023723-c93eff,
261110-024750-6ae64c are the second durability reviewer's probes (cited in its review file). The
stale reruns at strata:e0508e9 (9e3973, a9800f, 23571b) ran superseded code.

## Next

1. Read the jobs in State: build-test, both probe runs and mutants.py green, the four mutant jobs
   failing (in their tests, not their builds: the scripts exit 2 on a build failure, 1 on a test).
   Then fill F3's row in Confirmations and A5's evidence; for any red one, fix and rerun only it.
   The interrupted head jobs at strata:6ab3983 (A2 to A9's reruns) need `campaign job rerun` at
   strata:1ebeff8; the lead's call (they are outside this round's assignment).
2. The lead accepts each review with the Confirmations table's jobs (`campaign accept manifest-log
   durability --confirm F1=261110-073919-e779e6 F2=261110-073918-41b1cd`, `... security --confirm
   F1=261110-073919-a0029a F2=261110-073919-cf69ef`, fable's F1 with bb7303) and pushes the branch
   for CI's linux-asan-ubsan at strata:6ab3983.

## Blocked on

Nothing. The last Blocked on is answered by the lead's rulings L4 and L5 below.

## Evidence

All at strata:6ab3983 (the head). Figures in parentheses are the same jobs at strata:08a11ab,
whose code differs in `test/apply_atomic_test.cc` and commit()'s and install's post-commit fence (R39); the head's runs replace them when done.

| Criterion | Evidence |
|---|---|
| A1 | build-test 261110-142524-056dee (ctest 73 of 73 at 1ebeff87): manifest_log_test "appender" (one write per append, the truncate-back, one record in flight), index_test (commit, recover, `check()`, T4 cases 10 and 11), retention_test (D8 across a rollover), lifecycle_test; the retired files' codecs are gone (strata:074129c) |
| A2 | T4: build-test 261110-142524-056dee (lifecycle case 1, cases 4 to 11 in manifest_log, index, retention, lifecycle, apply_atomic; media_test every byte of the log) and CI's config 261110-142526-66f34b (assertions, clang ctest, libstdc++); T6: 261110-142525-cea487 (digests equal to strata main 3df7af4f on 80 streams) |
| A3 | manifest_log_test "replay cost" and "deleted sets" in 261110-142524-056dee and 261110-142526-66f34b (128 one-ordinal delete edits on 20M docs: 0.04 ms; at 2x D7's edit bound, 5M docs: clumped dense 1.5x the snapshot alone, sparse 1.7x; bound 4x); T7 row 261110-142527-2d21c1 (strict); the review's replay probe 261110-142526-c935bf (worst ratio 1.5x at 60M docs, 2x bound); mutants 261110-142526-2c3e04: "declaration names scanned per declaration", "identities scanned per edit", "a delete edit walks the whole set (A3)", "a dense neighbour walks the words (SR2)", "ordinals merged into one run per insert (SR2)", "no bounds on read" |
| A4 | manifest_log_test "deleted sets" in 261110-142524-056dee (bitset 125,000 bytes to 127,496 allocated; deltas 2,007 to 8,384, dense deltas 1,000 to 8,536); fuzz 261110-142525-7011d0 (ASan+UBSan, both log targets 301 s each, no crash; 673,092 and 1,683,207 runs at 1ebeff87); coverage 261110-142526-fd57f7; mutants 261110-142526-2c3e04: "a bitset decodes to ordinals (D42)", "a deltas body decodes to ordinals past the threshold (D42)", "a set bit's summary not marked" |
| A5 | F3 (a held view across every allocation failure of commit and install): apply_atomic_test in 261110-142524-056dee and ASan+UBSan 261110-142526-6bda18; open's half: manifest_log_test "allocation failure" in 261110-142524-056dee (63 points, 63 bad_alloc, 0 misreported); writer's half: apply_atomic's commit sweep, alone and with the next allocation failing too, an unfenced failure leaving the logs unchanged (R37) and failures after the commit fencing (R39), in 261110-142524-056dee and under clang and libstdc++ in 261110-142526-66f34b; mutants 261110-142526-2c3e04: "the head's replay fences only on damage", "the log's barrier fences only on a system error", "a durability fence set only once its message is allocated", "a failed barrier not truncated back" |
| A6 | build-test 261110-142524-056dee: manifest_log_test stage "caller rules" |
| A7 | build-test 261110-142524-056dee: manifest_log_test "crash images" and "bit flips" (35,200 flips, 0 silent), media_test (every byte of the log) |
| A8 | D57 (PLAN.md): T1 to T3, T5, T7 to T9 are milestone checks the lead runs on main after landing |
| A9 | 261110-142526-8c8521: prospect ff99494 (manifest-log-prospect) suite and churn/dump identity against strata main, then Xapiand master Xapiand:3ed4270d3's suite, on strata:1ebeff87 |
| A10 | D41 (PLAN.md, 2026-11-09): Q1 the manifest log first, Q2 a count of generations with `fallback()` reporting the damaged generation, Q3 accepted; folded into `docs/design/manifest-log.md` at strata:49ab291 |

## Confirmations

Each blocking finding: the job that confirms the fix at the head, and the same probe at the
reviewed commit strata:e0508e9 (fixed commit strata:08a11ab in parentheses where it already ran).

| Finding | At the head (strata:6ab3983) | At strata:08a11ab | At the reviewed commit (strata:e0508e9) |
|---|---|---|---|
| durability F1 | rename probe 261110-073919-e779e6; T4 case 10 and mutant "the open syncs the log only" in 261110-073918-41b1cd, 261110-073919-5715ab | 261110-032812-8afcbd: "NO LOSS (0 acknowledged commits lost)", every case 0 of 64 | 261110-022246-d64be6: "LOSS (128 acknowledged commits lost over the no-file cases)", 32 of 64 seeds per case, 14 opens "no manifest log" |
| durability F2 | build-test 261110-073918-41b1cd (T4 case 11 under faultfs `fault::keep`; faultfs_test's keep cases) and mutants 261110-073919-5715ab ("a failed barrier not truncated back" killed by index_test) | 261110-032812-0a35ed, 261110-032812-9c02e2 (that mutant killed) | none: the reviewer's `sync_error_probe` (in d64be6) writes the failed edit's image by hand and prints the same result at any commit (8afcbd shows it unchanged); the mutant is the fix reverted |
| security F1 | replay probe 261110-073919-a0029a; manifest_log_test "deleted sets" at 2x the edit bound in 261110-073918-41b1cd | 261110-032812-618943: the open's read at most 1.5x the snapshot alone over 18 rows (5M to 60M docs, 1x and 2x) | 261110-022216-4197e2: sparse below the threshold 796.8x at 20M docs, 2x (3,545 ms against 4.45 ms) and 924.7x at 60M (19,802 ms); clumped dense 295.1x at 60M |
| security F2 | fable's probe 261110-073919-cf69ef (exit 0 expected); index_test's sustained-ENOSPC case in 261110-073918-41b1cd; mutant "an unforced rollover's ENOSPC owes nothing" in 261110-073919-5715ab | 261110-032812-618943 `enospc_bounds`: "reopen: ok", 128 commits acknowledged, 172 refused ENOSPC, 272 live = 400 - 128 (R38) | 261110-022216-4197e2 `enospc_bounds`: 300 of 300 acknowledged, "reopen: FAILED: manifest log: past its bounds at generation 258", exit 3 |
| fable F1 (= security F2) | 261110-073919-cf69ef | 261110-032812-3886e3 exit 0: 129 acknowledged, 271 refused, 0 rollovers, plain open reads generation 259 (acknowledged 259), digest equal | 261110-023641-b98c95 exit 1: 400 acknowledged, 0 refused, plain open "past its bounds at generation 387", `open(to=386)` discards 144 acknowledged generations |

## Result

Ready for acceptance at strata:6ab3983, pending the jobs in State. Fix round 1 (strata:c78f870
to strata:08a11ab) fixes durability F1 and F2, security F1 and F2, and fable F1 and F2, and fuzzes
security F3's `to` paths; strata:8b532c8 closes the mutant gap in apply_atomic's sweep (R37), and strata:6ab3983 fixes the defect it found (R39). At
strata:08a11ab: 37 mutants, 36 killed; build-test 73 of 73; replay at most 1.5x the snapshot
alone where the reviewed commit took up to 924.7x; sustained ENOSPC refuses commits instead of
acknowledging a log the reader refuses; no acknowledged commit lost after a half-synced rollover.
Docs: strata `docs/design/manifest-log.md` (D7, D8), `docs/mechanisms/index-on-the-log.md`,
`docs/mechanisms/manifest-log-codec.md`.

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
- R29 (CI: install's figures): a segment born since the head takes its live length from its body
  only when its deletes are dirty; with deletes and none dirty (a lazy install's records, no
  body, D9) it is the record's `live_length`. cold_segment's install case asserts without it in a
  build with assertions; a check of the install's live figures and the mutant "an installed
  record's live length read from its body" hold it.
- R30 (CI: libstdc++): `stable_bytes(std::size_t)` is no longer noexcept. Proof: CI's config job
  puts the noexcept back and apply_atomic must terminate under g++-16; with it removed the sweep
  passes under libstdc++ (157 points).
- R31 (CI: TSan and timing): manifest_log_test replaces operator new only without TSan (as
  apply_atomic does) and then skips its two counting checks; its time bounds scale by 10 under ASan
  or TSan (CI's ASan run took 552 ms where the macOS release run took 168 ms; 5 s stays far under the 8.9 s it guards),
  and its two ratios take the fastest of five runs each.
- R32 (A5, found while fixing R30): `durable()` and the head's replay set `fenced_` before building
  the fence's message, so an allocation failing there too still fences. apply_atomic's sweep runs
  a second pass with the next allocation failing as well; without the fix it fails (N=140
  unfenced, the retry refused). The first index mutant's text follows the new line.
- R33 (security F2 / fable F1: which fix): both of the reviews' options, as one rule, "the writer
  never acknowledges a log its reader refuses". The ENOSPC rollover owes the rollover, so D8's
  "the rollover waits one commit" holds as written and the caller learns the disk is full at the
  next commit (space_error, index unchanged) instead of the log growing; and the waiting edit
  appends only within `log_within(..., 2)`, the function `read_log` applies, because an edit can
  itself be larger than the room between 1x and 2x (a delete edit's deltas can exceed the
  snapshot's bitset). Forcing the rollover alone would leave that gap; refusing at 2x alone would
  let every commit up to 2x skip the rollover under ENOSPC. The 2x guard has no test of its own: an
  honest edit that large needs a state built for it; the sustained-ENOSPC test holds the owed
  rollover.
- R34 (security F1: the representation, not the bound): D42's forms and memory bounds stay. Words
  gain a summary, a bit per nonzero word up to one word (1/63 more; A4's bitset bound now
  `body + body/63 + 64 + slack`); ordinals become ascending runs, each more than twice the next (at
  most 64), merged from the tail, so each ordinal moves O(log n) times and a lookup is a binary
  search per run. A merge's allocation failure leaves the set whole with one run more (apply() and
  its callers already drop or fence on any failure). `operator==` compares members, so a replayed
  state equals the writer's however its runs fell.
- R35 (durability F1: option a): the open syncs the directory before the log, the reviewer's
  recommendation: the open must not report a generation a power loss can revoke, which option b
  still did. Cost: one fsync(dir) per writer open, unmeasured here (the open-cost row is T-row
  work, the lead's after landing per L5).
- R36 (durability F2: faultfs's model): `fault::keep` marks every page differing from the durable
  bytes as lost at the failed sync; a later sync or ordering point makes the current bytes durable
  except lost pages, which keep their durable bytes (zeros past the durable size); a write or
  truncate dirties its pages again, and a crash reads lost pages' durable bytes. The appender stays
  broken after the undo, so the index still fences on a failed sync.
- R37 (the A5 survivor, not equivalent): the mutant leaves `fenced_` unset when the fence's own
  message fails to allocate inside `durable()`. On the barrier, by reading, the round's undo
  (strata:f9fdcca) and its own `sync_data` take the burst's second allocation, so the fence's
  message allocates and the barrier still fences; this explains why it survived at strata:08a11ab
  where it was killed at strata:e0508e9 (not measured). Its other sites stay
  live: the post-append step (the head's file names for retention) and the rollover's rename and
  directory sync. There the edit or new log is durable, the writer is unfenced, and the retry
  committed nothing new and matched the twin's digest, so the sweep passed. The sweep now requires
  an unfenced failure to leave every `manifest-<g>` byte-identical (SR4's "unfenced, the index is
  as it was" includes its log). At strata:8b532c8 the mutants job killed 37 of 37, and build-test failed: the new check caught the code itself (R39).
- R38 (security F2's probe at the fix): `enospc_bounds` exits 1 at strata:08a11ab on its line 54,
  `contents(again) == before`. `before` reads the index's memory, which holds the 172 deletes whose
  commits were refused; close drops uncommitted transactions. The reopen holds 272 = 400 - 128
  live documents, every acknowledged delete and nothing refused, so the check expects all 300
  commits to be acknowledged, which the fix refuses by design. The confirmation is fable's probe
  (digest equal to the acknowledged state) and index_test's sustained case.
- R39 (A5, found by R37's check): after `commit_files`, commit() published the view, freed views,
  ran retention and a reclaim slice, and install the first three, all outside `durable()`. An
  allocation failing there (retention's maps and sets, the view's vectors) threw bad_alloc with
  the edit durable and the writer unfenced: the caller saw a failed commit that had taken effect,
  and retention could stop halfway (`retain_owed_` already cleared). Of the two outcomes the test
  allows, fencing is the one the rule already gives everywhere past the log's sync ("any failure
  after the sync fences", index-on-the-log); swallowing would hide an allocation failure. Cost: a
  writer that runs out of memory in retention reopens instead of continuing. `reclaim_files`
  still keeps its own system errors (the file stays queued), so only what escaped before fences.

- R40 (durability F3, fix round 2): `publish_view` published the new view's pointer, then retired
  the old one with a `push_back` that could throw: the old view was freed under its reader and the
  new one freed while the registry named it. `basic_retired::reserve()` makes room for the next
  retire (geometric, kept across reclaims) and `retire()` is noexcept on that room, asserting it;
  `publish_view` and `retire_current` reserve before publishing. Cost on the commit path: one
  capacity compare, an allocation only when the list's capacity is reached (not measured; no
  timing claim). A retire without room terminates instead of freeing a held pointer.
  hazard_test's owners reserve too. apply_atomic's sweep reads through a view held across the step
  and through a fresh acquire after each failure.
- R41 (N2, optional, taken): the sweep is over steps that move the log, not only commit: a commit,
  a commit reclaiming a `force_merge(1)`'s sources (a non-empty reclaim queue), and an install,
  with a held view for commit and install. Mutant refs M2 and M3 carry the re-check's texts.
  Scripts: `.scratch/f3/asan_tests.sh`, `rel_tests.sh`, `mkmutant.py` in the strata worktree.

## Follow-ups

- Durability N1 / fable F3: a short `pwrite` count on a full disk is EIO (fences) where D4 reads
  as ENOSPC; faultfs's capacity never writes part of a request. Owner's call.
- Durability N2: the append's ordering point sits outside `durable()`; safe (nothing in the log
  changed), a comment would help.
- Durability N3 / fable F5: macOS ordering rests on F_BARRIERFSYNC; `full_flush_ordering` is
  opt-in. Unchanged from `main`.
- Fable F4: `open(..., to)` at a generation in an obsolete log cut below its snapshot reads as
  `corrupt_index("no whole snapshot")`, not `generation_not_retained` (D6.8).
- Fable F6: APFS, ZFS and UFS size-versus-data crash semantics unchecked (D4).
- Security F3's rest: a non-default `readable_by` and recover()'s fallback are not fuzzed.
- Security F4: decoded metadata and declarations cost about 50x their bytes in map nodes; bounded.
- Security F5 (pre-existing on main): open_segment sizes its deleted bitmap from the log's
  doc_count before the segment file's is checked.
- A process kill between a failed sync and the truncate-back leaves the failed edit's clean pages
  for a same-boot reopen (Linux reports the writeback error once): no writer-side fix exists; an
  open could compare against a re-read with O_DIRECT, out of scope.
