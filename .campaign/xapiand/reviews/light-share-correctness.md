# light-share review: correctness

Commit: Xapiand:2b8c66958718
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: nothing under `src/` changes; the quest lands the A1 diagnosis (`docs/design/light-share.md`), the
harnesses and the prototype `harness/light_share_task.patch` (Kronuz/task `drain()`), on whose sweep the owner's
pick of a share rests. The landed mechanism is light-share-task's.

## Evidence I relied on

- Raw arms of X1b 261118-135313-0d4d30, X1c 261118-184044-457a2d, X1d 261118-184045-c76864 (exclusive):
  `harness/light_share_report.py` rerun on their `ls-*.json` reproduces the note's per-arm values. All three ran
  `build-ls/bin/xapiand` from build job 261118-022831-d00944 at 70e37e321; `git diff 70e37e321 2b8c66958 -- src
  CMakeLists.txt` touches only test registration, so the binary stands for the head.
- T12 jobs 261118-135316-88308e, 261118-184049-f10605 for RESTORE and QPS as printed.

## What I checked myself

Probe `.scratch/xapiand/review-light-share-correctness/run.sh` (+ `share_probe.cc`), job 261118-194913-c0d047: Kronuz/task at
Xapiand's pin plus the patch, clang TSan, then task's asio, lanes and deadline_asio tests with `LS_HEAVY_SHARE`
unset, 1 and 2, and three probes:
- `cap` (share 1 and 3): 8 loops, 6,400 heavy turns posted from 8 producer threads with light demoted ones mixed
  in; the most heavy turns inside at once, then share+1 turns on share+1 loops that wait to overlap (exactly
  `share` must, or a slot leaked).
- `parked` (off, share 1): loop A holds the one slot; loop B parks on a heavy head; a light demoted handle then
  reaches B. Does it run while A holds the slot?
- `bypass` (off, share 4): one loop, slots free, a heavy handle queued ahead of a light one that reposts itself
  demoted 1,000 times. How many light turns run first?

Results (job exit 0, no TSan report in any of the 15 runs, no hang):
- task's asio, lanes and deadline_asio tests pass with the share off, at 1 and at 2: the patch breaks no existing
  behavior and adds no race.
- `cap`: share 1, all 6,400 heavy turns ran, at most 1 inside; share 3, at most 3. Afterwards 1 of 2 and 3 of 4
  overlapped: no slot leaked, and the cap still binds.
- `parked`: off, the light handle ran at once. Share 1, it did NOT run during the 300 ms that A held the slot; it
  ran 0.26 ms after A released it (F1).
- `bypass`: off, the heavy head ran first (after 0 light turns). Share 4 with every slot free, it ran after all
  1,000 light turns (F2).

Code reading, `task_asio.hh` as patched (patch lines cited as the patched file's):

1. The count is exact. Acquire (`--free`), grant (transfer to the waiting front, `free` unchanged) and release
   (`++free` only when nobody waits) all run under `heavy_share::m`; a loop enters `waiting` only from `drain()`,
   which runs once per loop at a time because a parked loop keeps `draining_` set, so it is queued at most once.
   `free > 0` only with `waiting` empty, so no loop barges past a parked one: grants are FIFO.
2. No exception path: `e.h.resume()` cannot throw (task.hh:619 stores, task.hh:716 and :1791 terminate), and a
   cancel resumes the frame through the same lane. Heavy resumes after I/O take the lane (`asio_park::resume` ->
   `resume_on` -> `scheduler_ref::post` -> `post_heavy`), so every heavy turn on a loop passes the cap.
3. A slot leaks only in teardown: `~asio_scheduler` removes `this` from `waiting` but returns no slot it was
   granted and has not used, and `release_slot()` reads `w->executor_` after unlocking, which races a concurrent
   destructor. Xapiand's reactors live for the process (`reactor.h:320`, `manager.cc:179`), so not in Xapiand.

Q2, T5 per arm (loaded light query p99 ms / its limit 2x unloaded + 1):

| Share | Arms (job position) | p99.9 ms |
|---|---|---|
| 8 | 16.3/9.15 (X1b 2), 8.1/8.90 (X1b 5), 8.4/8.73 (X1d 3), 9.7/9.00 (X1c 6) | 56.5, 48.2, 43.6, 49.9 |
| 6 | 28.3/7.79 (X1c 1), 30.3/8.66 (X1d 2), 7.7/7.79 (X1c 4), 10.2/8.76 (X1d 5) | 74.0, 67.6, 60.0, 52.5 |
| 4 | 6.5, 6.1, 6.5, 6.9 (positions 2, 5, 1, 6) | 96.3, 69.2, 73.5, 91.4 |
| off | 40.7, 25.7, 58.2, 17.7 | 112.9, 56.9, 135.1, 57.2 |

Each loaded phase has 3,419 light queries: p99 is the 35th slowest, on the knee between p50 (1.4-2.1 ms) and
p99.9 (44-135 ms), so a few dozen slow queries move it 4x. Shares 6 and 8 are bimodal (about 8-10 ms or 16-30
ms); the note's 19.3 ms for share 6 is the midpoint of the two modes, a value no arm produced. 2 of 4 against 1 of 4
in bound is indistinguishable (Fisher exact p = 1.0). The two slow share-6 arms ran first and second in their jobs
and the slow share-8 arm second, but share 4 ran first and second without harm, so arm order is not established
either; the workdir is wiped per arm (`stall_load.py:242`). Reading: estimator noise at n = 4, not a shown
non-monotonic effect. Share 4 alone is robust (6.1-6.9 ms, 4 of 4).

Q3, T4: loop lag p99 is a bucket edge (1.024, 1.28, 1.54, 1.79, 2.05, 2.56 ms). Worst loop at share 8/6/4:
1.79-2.56 / 1.54-2.05 / 1.79-2.05; the median loop: 1.79-2.05 / 1.54 / 1.28-1.54, still falling. Node CPU over
the loaded phase: 245/218/191 s in about 21.8 s, 11.2/10.0/8.7 of 16 cores, with the load generator on the same
host. Unloaded HTTP loop lag p99 is 0.51 ms in every arm.

Q4: idle and light-only are unchanged (no heavy entry: `drain()` adds one relaxed load). With heavy work the
patched `drain()` differs even when slots are free (F2), and the cap binds on slots, not on CPU.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A light demoted handle that reaches a parked loop waits for a heavy slot. `demote()` posts `drain()` only when `!draining_`, and a parked loop keeps `draining_` set until a grant, so the light slice runs after the loop is granted a slot and has run its heavy head. Every request past its first 200 us slice is demoted (`src/server/search_views.cc:506-510`), so this sits on T5's path, and the sweep the pick rests on measured it. The note's "light handles never take a slot" holds, but they wait for one. Whether fixing it lets share 6 or 8 meet T5 (15-25% heavy cost instead of 36%) is unsettled. | `parked` probe (261118-194913-c0d047); patch `drain()` park branch and `demote()` | A parked loop still runs light demoted handles, for example `demote()` posts `drain()` for a light entry while parked and `drain()` then serves a light entry before it parks again. Test: `share_probe parked` under share 1 reports the light ran while the slot was held. Rerun the 8/6/4 sweep at 6+ interleaved arms per share (F3) before the note's pick. |
| F2 | no | defect | With slots free, a heavy head waits while any light entry is queued: `drain()` looks for a light entry before it asks for a slot, so a steady stream of light slices on one loop starves its heavy work without bound, where the unpatched FIFO runs it in turn. The note says a loop parks only when it "finds no slot". The sweep is not confounded: at share 12 the reorder is always on and the slots rarely bind, and 12 measures the same as off. Non-blocking here because nothing lands as code; light-share-task must not carry it. | `bypass` probe (261118-194913-c0d047) | Ask for a slot first and fall back to a light entry only when none is free. Test: `share_probe bypass` under share 4: the heavy head runs after at most one light turn. |
| F3 | no | gap | Q2: the T5 6-vs-8 inversion is noise in a p99 on the knee of a heavy tail at n = 4 (tables above), not a measured non-monotonic effect. The note's "T5 improves as the share falls" and the owner options' "8 in 2 of 4, 6 in 1 of 4" read more than the data hold, and the medians of bimodal arms hide the modes. The per-arm limit is also noisy (7.3-12.2 ms with the unloaded p99). | raw arms of X1b/X1c/X1d | In the rerun of F1: report each arm with its position, and a tail-robust measure beside p99 (the count of light queries over the limit, or the p99 with a bootstrap interval). |
| F4 | no | gap | Q3: "T4 is bounded by turn length, not CPU" is inferred, as the note says, and the plateau does not discriminate: under either model the worst loop's lag falls slowly as fewer loops run heavy turns, and the median loop does keep falling (1.79 -> 1.54 -> 1.28 ms). The settling measurement: a per-turn histogram of wall time and thread CPU time (`CLOCK_THREAD_CPUTIME_ID`) for each `drain()` resume on the HTTP loops under the share-4 mix. If the wall p99 is about the lag p99 (1.3-2 ms) and wall is about CPU, it is turn length; if wall is well above CPU, or turns are well under the lag, it is CPU. | lag fields of the 18 arms; node `cpu_s` | Run it before T4 is handed to the stall quests as their target, or keep the note's "inferred". |
| F5 | no | gap | Share 4's cost is understated for the owner's pick. Light query p99.9 at share 4 (69-96 ms) is worse than at share 8 (44-56 ms) and no better than off's median (about 85 ms); the note's table stops at p99. The heavy cost counts searches only, while RESTORE in the same mix gains 75% (80,000 -> 140,000 restored docs per loaded phase), which the note marks "not explained": the heavy work moves, not all of it is lost. | `p999_us`, `heavy.restored_docs` in the arms | Add p99.9 and restored docs to the note's table and to the owner options; may change after F1. |
| F6 | no | gap | Q4: the share applies on slot contention, not CPU contention. A heavy-only server with more than `share` loops busy and idle cores is held to `share` loops' heavy turns, and the landing plan sets a fixed count from the manager with no knob. T12, A4's evidence, replays term and range queries at 16 clients (`harness/loadtest.py:47-60`); nothing shows they reach the heavy lane (8 light slices), so its flat QPS may not exercise the cap. Owner's call. Options: (a) keep a fixed share and measure a heavy-only workload; (b) a share that binds only while light work waits; (c) keep it off by default. | patch `drain()`; T12 logs | For A4 in light-share-task: a heavy-only arm (heavy clients at, for example, 8 with no light load) at off and the chosen share, and a count of heavy-lane turns in T12. |
| F7 | no | nit | Teardown: a scheduler destroyed with a granted, unused slot leaks it, and `release_slot()` uses `w->executor_` after unlocking, which races a destructor. Not reachable in Xapiand (process-lived reactors); in Kronuz/task, where tests churn schedulers, it would deadlock or use freed memory. | patch `~asio_scheduler`, `release_slot()` | light-share-task: post the grant under the lock, or have the destructor return a held slot; a test that destroys a granted scheduler. |
| F8 | no | nit | Debug builds bypass the share: `Counted` (`src/engine/host_asio.h:163-164`) posts heavy resumes through `post_demoted`, so Debug tests never exercise the cap. | `host_asio.h:164` | light-share-task: count heavy turns in `Counted`'s pump, or test the cap in Kronuz/task. |
| F9 | no | nit | Parked loops hold their heavy handles longer, so `demoted()` climbs toward `DEMOTED_ADMISSION_CAP` (256, `search_views.cc:3114`) at a lower load and 503s start sooner. The runs saw no 503. | `search_views.cc:3114, :3135` | light-share-task: note it, or exclude handles parked for a slot from the admission count. |

## Not run

- No rerun of any T4/T5/T10/T12 timing (exclusive only, and F1 changes what would be measured).
- No TSan build of xapiand itself: the cap lives entirely in `task_asio.hh`, which the probe builds under TSan with
  task's own asio tests.
- T10 arms not re-read beyond the journal's values.
- Linux and FreeBSD: CI's; the probe ran on macOS arm64 only.
