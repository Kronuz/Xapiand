# faultlab-core review: correctness

Commit: faultlab:a5dcc10ac03cc3aef68efb980ebcb36b351a0fba
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261118-010005-2aea3a (Release build and ctest, 15/15, at a5dcc10): the suite passes. I did not rebuild it.
- Mutant jobs 4b6402 (stream-shift, A2), 9c42c3 (no-sector-cut, A3), 596e36 (link-orphan, A4),
  2f7a32 (recorder-bytes), 67f979 (proxy-close-swallows), db2445 (selftest monotonic, seal) for A5:
  done, each fails its test. 37fe70 (additions) and 3d76e7 (forced-skips-draw) finished done (A1);
  a9f786 (fold-mutates) and 5b5cd0 (kill-keeps-leases) were still running when I wrote this; I
  read their diffs (below) but not their results.

## What I checked myself

Probe job 261118-011556-f739ef at a5dcc10 (exit 1: the two P1 asserts fail; everything else passes), scripts in
`/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-faultlab-core-correctness/` (`run.sh` runs all):

- `probe.cc`: P1 a shared-mapping store issued after the power cut; P2 a mapped store then
  `File::sync`; P3 (R7) two 100-byte appends; P4 an unsynced overwrite a->b->c of one sector;
  P5 link, link, remove; P6 a replace over a name whose link dropped.
- `scenario_lines.cc` + `history_probe.py`: C++ scenario lines round-trip in C++ and through
  `scenario.py`; history.py on nine hand histories (legal concurrent seal reads, a seal read behind
  a completed read, null after an acknowledged seal, an `info` write setting no floor, monotonic
  forward and behind across a restart, the same without `durable`, a stale linearizable read).
- `strata_seeds.sh` rerun against strata:491a1bd and completion:d8b538c, compared byte for byte
  with `test/strata_seeds.inc`.

Results (job log):

- P1: a store after `power_off_after(0)` survived 28 of 64 seeds; after a power-off EIO, 28 of 64;
  the failed write itself, 0 of 64 (control).
- P2: a mapped store then `File::sync` was lost in 0 of 64 seeds.
- P3: two 100-byte appends reach sizes {0, 200} under both the sector pick and the coin; 100 never.
- P4: an unsynced a->b->c overwrite of one sector reaches {a, c}; b never.
- P5: link a b; link a c; remove a reaches {a}, {a b}, {a b c}, {a c}, {b c}: never nameless, and
  {b} and {c} (a remove kept while a link before it dropped) are excluded, as R1 intends.
- P6: a replace over a name whose link dropped never loses the old file (512 seeds).
- Scenario: four C++ lines (all picks, max seed, extras, `sector`) round-trip in C++ and print back
  identically from `scenario.py`.
- history.py: all nine verdicts as expected (four accepted, five rejected with the right check).
- Seed table: regenerated from strata:491a1bd, identical to `test/strata_seeds.inc` (384 rows).

Read by hand: faultfs.h against strata's faultfs.hh (crash, namespace replay, data rebuild), io.h
at completion:d8b538c (File and IO contract), recorder.h, scenario.h/.py, events.h, history.py's
`_monotonic` and `_seal`, the nine mutant diffs.

## Answers to the lead's questions

1. **States.** Default `crash(seed)` replays strata's coin stream exactly (P-seeds). sync,
   sync_data (= sync), sync_files/sync_data_files/sync_files_and_directory (io.h defaults, per file
   then the directory), order_files_and_directory, link no-clobber (live EEXIST, faultfs.h:1105;
   replay drops a link whose target is taken, :606), preallocate (a mutation, capacity first,
   zeros), leases (released at close, kill, crash) and sector cuts are as the design states. Two
   departures from io.h: F1 (more states: post-cut mapped stores survive) and F2 (fewer: mapped
   stores always durable after sync, io.h:123 says not on macOS). Fewer states than a real
   filesystem by the design's choice (F3): write-boundary sizes (R7, P3) and intermediate versions
   of an overwritten sector (P4); see N1.
   **R7** is both: the write-boundary *size* (a writeback between two appends inside one sector) is a
   real-filesystem state faultfs misses (N1, an owner model question, not a defect here); the
   sub-sector *byte tear* (CrashIO tears an unaligned write at offset+512, test_crash.cc:94-96,
   174) is CrashIO's over-reach, since a device tears at sector boundaries. storage writes whole
   blocks (storage.h:565, 988, 1356), so step 9's CrashIO states are covered by the block-append
   half of A3.
2. **Port.** The rebuild is strata's line for line (faultfs.h:528-627 vs strata faultfs.hh:250-334);
   the link coin is drawn only for a logged link, so link-free seeds keep their stream. The seed
   table is not vacuous: 384 rows, regenerated from strata itself (P-seeds), 130 distinct states
   over six scenarios (29, 32, 21, 2, 2, 44); scenarios 3 (lost pages) and 4 (truncation) reach
   only 2 states each (N4).
3. **verify().** It compares every name, directory and byte of the shadow's current state with
   the real tree, so a raw write, link, rename, truncate or growth past the shadow is caught;
   recorder_test covers a raw write and a raw link, and recorder-bytes kills the byte compare.
   A short read changes no state, and the shadow mirrors a short real write exactly
   (recorder.h, `write_at`). What it cannot see is inherent: a raw sync, a raw write of identical
   bytes, mapped words (by design, length only).
4. **Formats and checker.** The scenario line round-trips both ways; events.h's lines load in
   history.py (events_test runs the C++ log through the checker). The seal and monotonic checks reject the
   non-linearizable and behind histories and accept the legal concurrent ones.
5. **Mutants.** Each changes behavior, none a message: additions (`kind == create`),
   fold-mutates (a fold counts a mutation), forced-skips-draw (a forced coin skips its draw,
   shifting the other fields), kill-keeps-leases, link-orphan (drops R1's remove rule),
   no-sector-cut and recorder-bytes (disable a whole feature, coarse but real), stream-shift
   (seed + 1), proxy-close-swallows, selftest monotonic/seal (disable the check). R1's replace
   half (faultfs.h:614-616) has a test ("a replace over a linked name cannot outlive its link")
   but no mutant; only one per fix is required.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A store into a shared mapping made after the power cut survives `crash()`: folds keep running while `powered_off()` (every call, and `rebuild` itself folds first), so a program's error path that stores after its EIO leaves bytes no power loss can leave. The design promises only "stores made before power-off can survive". | P1, job 261118-011556-f739ef: 28/64 seeds; faultfs.h:337-339, 528-529, 641-655, 1024-1029 | Fold nothing once `power_left_ == 0` (the cut is the instant power runs out); a test: store after `power_off_after(0)` and after a power-off EIO, then crash with `sectors = fresh`, and the store never survives. |
| F2 | yes | gap | io.h:123 (completion:924c6a6) promises mapped stores durable after `sync` "everywhere but macOS"; faultfs makes them durable at every `File::sync` (a fold then `durable = flushed()`), so it never reaches "synced, mapped store lost". A consumer that relies on `sync` for mapped words passes faultfs and can lose them on macOS. faultfs is the one portable model; which contract it models here is the lead's/owner's call. | P2, job 261118-011556-f739ef: lost in 0/64 seeds; faultfs.h:912-937; io.h:121-124 | Options: (a) model the portable contract: a sector a fold changed stays tearable at a crash until rewritten through `write_at` (or until an explicit msync member exists); (b) keep it, and state in faultfs.h's header and crash-model.md that mapped stores follow Linux and FreeBSD, macOS out of model; (c) a `filesystem` switch. Recommendation (a) or (b); either needs a line in the docs and, for (a), a test. |
| N1 | no | gap | faultfs keeps two versions per sector (durable, current) and sizes at sector boundaries; a real writeback between unsynced writes can also persist a write-boundary size (P3: size 100 of 200 not reached) or an intermediate version of an overwritten sector (P4: `b` of a->b->c not reached). io.h does not exclude either. F3 rules faultfs's model as the model, so this is the owner's model question, not this quest's defect; consumers that append headers and bodies in separate writes inside one sector never see the header-only state. | P3, P4; crash-model.md "Why sector tears" | Owner: accept and document both in crash-model.md, or a later `size_pick::writes` / per-write version cut. |
| N2 | no | gap | A3 is narrowed: its CrashIO half covers block appends only (R7), not "random append sequences" as the quest file says. Justified (storage writes blocks), but the lead should accept the narrowing explicitly. | sector_test.cc:131-137; journal R7 | Lead: accept R7 in the quest's evidence. |
| N3 | no | nit | crash-model.md ("fixing one family never moves another's draws; only a size pick changes how many sector coins follow") and faultfs.h:526-527 overstate: an epoch pick also changes how many epoch coins are drawn (the loop stops at the first false coin, faultfs.h:542), shifting every later draw. States stay legal; only the stream claim is wrong. | faultfs.h:540-542 | Say "a size or epoch pick changes how many coins follow". |
| N4 | no | nit | The seed table's lost-page and truncation scenarios (3, 4) reach 2 states each over 64 seeds, so they pin little of those paths. | strata_seeds.inc | Optional: richer scenarios 3 and 4 in a later table, re-recorded from strata before step 3 deletes it. |
| N5 | no | nit | `size_pick::sector` also cuts between sizes on a pure shrink (durable 3000, unsynced truncate to 1000: sizes 1024..2560 of old bytes), a state a single truncate never leaves; the design's wording ("from the durable size to the current one") allows it, and it only adds states. | faultfs.h:569-576 | Optional: cut only above the lower of durable and the size before the first unsynced truncate. |
| N6 | no | nit | Mutants sit on 4923aa4, not a5dcc10; faultfs.h gained only `model_access` (+18 lines) since, so they still test the landed code. | `git diff 4923aa4 a5dcc10 -- include/faultlab/faultfs.h` | None. |
| N7 | no | nit | A parsed line with `=` inside a value (`k=a=b`) parses in both halves but cannot print back (`line()` refuses `=`); events.h writes bytes >= 0x80 raw, so a non-UTF-8 key breaks the Python reader. | scenario.h:57-61, 80-83; events.h `quote` | Refuse `=` in values at parse; escape or refuse non-UTF-8. |

## Not run

- CI on Linux, macOS, FreeBSD (A6's second half; the repository's first CI was running).
- Sanitizers locally, TSan on the recorder's mutex and on folds.
- The results of mutant jobs a9f786 and 5b5cd0 (A4), still running.
- proxy.py beyond its own suite and mutant; netlab and disklab are not in this quest.
- Multi-directory crash states beyond faultfs_test's cases (R2's device-wide settle reviewed by
  reading only).
