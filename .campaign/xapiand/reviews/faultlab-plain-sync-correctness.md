# faultlab-plain-sync review: correctness

Commit: faultlab:eefba5364dcdd24da856a2f51cb2dad28bc3ca40
Verdict: CHANGES
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261124-084011-18ae99 (build-test at eefba53, exit 0): every suite (faultfs, model, policy,
  seeds, sector, recorder, scenario, events, demo, Python) passes. `test/model_test.cc` is the only
  test file changed, and it only adds cases (the one removed line is a header comment), so the
  existing cases pass unchanged against the new headers. This is A2's evidence.
- 261124-084008-5de615 (mutant faultlab:f0426b2, `persists(fsync)` true on macOS, `--fails-without`):
  exit 1, 7 failures, including the named case "macOS fsync (plain): a kill keeps the synced
  bytes, a crash may drop them". The mutant is killed, so A1 has its evidence.

## What I checked myself

Scripts are in `.scratch/xapiand/review-faultlab-plain-sync-correctness/` (`run.sh` builds them with
CMake and Ninja against main 5d4df9b's and the quest's `include/`, with completion d8b538c). They
ran as one job, 261124-090034-30a3b0 (light, exit 0, 60 s).

- Q4, the default is unchanged (`diff_driver.cc`). The probe ran 4000 random single-threaded
  sequences in the default mode. They cover create/open, writes, truncates, `sync`, `sync_data`,
  `sync_directory`, ordering points (a file listed twice included), replace, remove, `kill`, sync,
  sync_directory and write faults (before and after, with `keep`), and power_off_after/power_on.
  Each sequence then got 24 crash seeds under two policies, plus kill-then-crash. That is 200000
  state hashes, and they are identical between main and the quest. In one thread the default
  path is byte-for-byte unchanged, including the `as_of` preference flip, `stage()` replacing at
  one epoch, `stage` erasing `at_mark`, and the restructured fault path in `flush()`.
- Q1, plain-mode soundness (`plain_order_probe.cc`, macOS plain on this host). 3000 sequences of
  2 to 5 rounds ran, each round creating r<k>, pushing it four ways (plain fsync, sync_data, a
  listed F_BARRIERFSYNC point, or both), then a barrier. Noise writes and plain syncs went to
  `base`, and some runs had an unordered last round. That gave 192000 crashes.
  - **O** (a file created after barrier k-1 exists only if every earlier round's pushed bytes are
    whole): 0 violations.
  - **L** (all-lost picks give back the pre-plain durable state, so nothing plain persists):
    0 violations.
  - Every loss shape is reachable: every round lost or kept in prefix order, and partial (`p`)
    and empty (`e`) files.
  - So the model never keeps a plain-synced write a drive could lose, and never keeps a write
    issued after a barrier without what that barrier ordered.
- Q2, R4 (`r4_probe.cc`). The concurrent stray race is forced in one thread by a foreign
  `journal::File`, listed first in `order_files_and_directory`, whose `sync()` runs
  `sync_directory` while the faultfs loop holds no lock. The outcomes of file `a` (durable `o`,
  stray `A`):

  | Scenario | main | quest |
  |---|---|---|
  | R4a stray, then another file's full sync | A o | A |
  | R4b stray, a point, then that sync | A | A |
  | R4c stray, write X to sector 1, a new point, a later create | A A0 AX o (+later) | adds **o0 oX** |
  | R4d stray alone | A o | A o |

- Q3: I read completion-flush-mode's table at completion-wt 1a1a32c (`include/completion/posix_sync.h`
  `file_sync`, `file_sync_data`, `files_sync_devices`; `include/completion/posix.h`
  `order_files_and_directory`, `sync_directory`) against faultlab's `call_for`/`persists`
  (`include/faultlab/faultfs/types.h:73-98`). Every site matches today:
  - macOS full: F_FULLFSYNC.
  - macOS plain: fsync, with one F_BARRIERFSYNC on the directory's device at an ordering point.
  - Linux: fsync, with fdatasync for sync_data.
  - Elsewhere: fsync.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | `order_pushed` (`include/faultlab/faultfs/filesystem.h:294-302`) treats every entry of `strays_` whose staged epoch equals the new point's epoch as a plain push. But in full mode `strays_` also holds the race strays: files staged after another thread's sync settled their point, whose old epoch number aliases the new point's. Such a stray gets an `at_mark` at the new point, and `as_of` now prefers it, so a crash can keep a write made after the stray's ordering point returned while dropping the bytes that point staged (`oX` in R4c). The model's own invariant forbids that state ("a write after the point never survives without the staged tail before it"), and main never reaches it. The model errs strict, not kind: a concurrent sweep may report a corruption no device can produce. It is still a change for existing full-mode users, which the quest's contract rules out. | 261124-090034-30a3b0, R4c row; `r4_probe.cc` | Give `order_pushed` only what `push()` staged (for example a `pushed_` set beside `strays_`, cleared where `strays_` is). Add a full-mode model_test case with the R4c injector asserting that no crash keeps X without `A`. |
| N1 | no | gap | R4 (Q2): ruled sound. A persisting File::sync settling strays when no ordering point is open matches F_FULLFSYNC, a drive-wide cache flush, and Linux fsync, whose preflush covers completed writes. In full mode the stray was F_FULLFSYNC'd or fsync'd at its own point anyway. Main already settled strays on any File::sync when a point was open (R4b), so this removes an inconsistency and weakens nothing real. It only changes outcomes in the race (R4a: main `A o`, quest `A`); single-threaded sweeps are identical (Q4). | 261124-090034-30a3b0 (R4a, R4b, differential) | None. The CHANGELOG "Changed" entry already names it. |
| N2 | no | gap | Drift (Q3): faultlab's tests check `sync_call_at` against `call_for`, i.e. the table against itself, never against the calls PosixIO makes. The direction that hides loss is completion weakening a call (say, full-mode `sync_data` to fsync) while faultlab still says it persists. completion-flush-mode's own A1 mutants guard completion's side, but nothing ties the two tables together. A single shared enum also needs completion to move `FlushMode` out of `posix_sync.h` (`enum Value { plain, full }`, a class rather than faultlab's enum class) into `io.h`, the only completion header faultlab depends on. | `include/faultlab/faultfs/types.h:58-98`; completion-wt 1a1a32c `include/completion/posix_sync.h:43-50` | Not blocking: completion:flush-mode has not landed and today's tables match. In journal-sync-mode, or the lead's one-table change, add a test that records PosixIO's calls per seam operation (`replace_sync_calls`) and compares them with `call_for`. |
| N3 | no | nit | The model takes F_BARRIERFSYNC as ordering on every drive. completion documents the order as guaranteed only on internal Apple SSDs and warns otherwise (completion-wt `include/completion/posix_volume.h:91-102`). On other storage the plain model is too kind about ordering. | `docs/mechanisms/crash-model.md` (Plain and full syncs) | Say in crash-model.md that the plain model holds where the barrier orders (Apple SSDs). |
| N4 | no | nit | R5 cost: in plain mode `strays_` is never cleared, so each ordering point's `order_pushed` walks every file ever pushed, and every point and push keeps a staged copy until a crash. Long plain sweeps grow by points × pushed files. The R5 docs mention the accumulation, not the per-point walk. | `filesystem.h:294-302` | None needed now. |

## Not run

- Real concurrency (two threads) and TSan. The race was forced in one thread through the
  foreign-file hook.
- Directory-level `settle_directory` with subdirectories in plain mode. The probes used the root
  only.
- Shared mappings (`map_shared`) under plain pushes.
- Linux or FreeBSD hosts. `host_platform` there was checked only by reading.
- Downstream consumers' sweeps (journal, storage CrashIO) against this commit. Q4's differential
  stands in for them in single-threaded use.
- Session: this ran as a subagent of the lead, not a session of its own, so no
  `xapiand · reviewer faultlab-plain-sync correctness` title applies.
