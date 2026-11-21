# turn-log-parity--checkpoint-window: journal

State: waiting on jobs 261121-054157-2f69f9
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

D2 item 2 built (strata:1e42abcd): the sealer keeps the window ((retain_segments - 1) x
segment_bytes) and an atomic writer count; an index counts in at its first join after a
checkpoint, out at the checkpoint and at detach; `log_due` is `logged_bytes >= max(floor,
window / writers)`. `checkpoint_floor_bytes` (default 1 MiB) replaces `checkpoint_log_bytes`.
Jobs queued (all behind a VM hold until about 23:20):
- 261120-204250-36020e: D2's test at the real trigger, plus the turn-log tests (ctest -R).
- 261120-204252-7f2e9c: the window mutant (strata:5c7222de, `log_due` at `command::max_bytes`), must fail.
- 261120-204253-47f1cc: the forward-copy mutant rebased (strata:810411b8), must fail.
- 261120-204255-044c67: the probe at strata:1e42abcd with the default floor, to match the 1 MiB row.
- The eight floor jobs at strata:14b476d1 (first one started, waiting for its exclusive hold).

## Next

1. 261121-054157-2f69f9 (the test at the fix) passes: A6 stands with 261120-204252-7f2e9c as its
   mutant; the task is ready to merge. If it fails, read its log first.
2. Fill the floor table from 261121-051304-224e7a and the floor jobs.

## Blocked on

Nothing. msg (lead ruling, step 2): option 1 taken, as D2 is written; turn_seal.hh added to the
file set. Option 2 (checkpoint once the log moved a window past the group's checkpoint, with the
floor) stays an alternative, measured only if the floor table or a skewed workload shows option 1
checkpointing far more than the log's movement needs: under Zipf, one hot group among 10k cold
ones checkpoints every floor under option 1, every window under option 2.

## Evidence

| Criterion | Evidence |
|---|---|
| A6 (D2's test) | 261121-050248-2f7f8d at strata:1e42abcd (was 261120-202804-e233bb at strata:14b476d1: at most 5 segment files (bound 6) over segments 1..41, 24 records forwarded (115 KB) for 39.9 MB of commands, 12 writer checkpoints at a 3 MiB window) |
| A6, fails without the forward copy | 261120-204253-47f1cc at strata:810411b8 (`mutant/turn-log-parity--checkpoint-window-forward`, child of the fix, journal's `forward_slice` patched to return nothing), exit 1; earlier 261120-203457-3324d7 at strata:71d3a338 |
| A6 (D2's test at the fix) | 261121-054157-2f69f9 at strata:1e42abcd; 261120-204250-36020e at strata:1e42abcd with the turn-log tests (ctest, 5 of 5) |
| A6, fails without the window | 261120-204252-7f2e9c at strata:5c7222de (`mutant/turn-log-parity--checkpoint-window-window`, child of the fix, `log_due` at `command::max_bytes`): 7 segment files (bound 6), 30.0 MB forwarded for 5.1 MB of commands, 0 writer checkpoints, exit 1 |

Floor table (pending the eight jobs): 10k indexes, 8M puts.

| Trigger | puts/s | checkpoints | checkpoints/s | log written | log zero-fill | forwarded | index written |
|---|---|---|---|---|---|---|---|

Smoke (light, not evidence), 261120-202808-b1442b: 10k indexes, 1M puts at a 1 MiB trigger: setup
7.3 s under nosync, reopen 0.3 s, run 4.5 s at 220k puts/s, 20 checkpoints (4.4/s); run bytes:
log records 131.6 MiB, log zero-fill 124.0 MiB, index 5.0 MiB; 60.2 MB forwarded.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- 261120-202805-29d178 is withdrawn as evidence: it was the "window mutant" only because it ran at
  strata:14b476d1, before D2 item 2 existed. `campaign job rerun` moved it to strata:1e42abcd,
  the fix itself, so its rerun 261121-050249-f9d136 ran no mutant and passed, as the fix must.
  The test kills the real mutant (strata:5c7222de, a commit on top of the fix): 261120-204252-7f2e9c.
  Test unchanged.

- The floor rows run at today's code with the trigger fixed by `checkpoint_log_bytes`: at 10k
  writers D2's share is the window over 10k (12 MiB / 10k = 1.2 KiB with kv's 4 MiB segments), so
  the trigger after D2 item 2 is the floor exactly. The "before" row is today's 64 MiB default.
- The probe's buffer share is 256 MiB, so the trigger, not the buffer, ends a record run. With
  kv_strata's share (budget / indexes, at least 64 KiB) every index flushes at 64 KiB at 10k
  indexes, and no floor of 256 KiB or more is ever reached: the floor binds only where an index's
  buffer share exceeds it.
- Uniform writes over 10k indexes reach no floor in a 10-minute run (each index logs ~100 KiB per
  1M puts), so the probe writes by Zipf 0.99, each of 4 threads over its own 2.5k indexes, after one
  put to each index so all 10k are writing groups.
- Setup creates under nosync.c and exits with the groups open; the measured process reopens them.
  With real syncs 2000 creates took 135 s (one group create at a time in the host).
- Checkpoints are counted where an apply's flush emptied the buffer; merge commits are not.
- D2's test mutant for item 3 patches journal through FetchContent's PATCH_COMMAND
  (`cmake/mutant-no-forward.cmake`, on the mutant branch only), in its own build directory.
- The test asserts, beyond the segment bound: the log advanced 10 x retain_segments segments,
  something was forwarded, forwarded bytes at most a tenth of the commands, writer checkpoints
  within one of commands / window, and the idle index reads its write.
- The test runs at the real trigger only (`checkpoint_window`); its explicit mode went with
  `checkpoint_log_bytes`. The window mutant is a commit on `mutant/...-window`, the forward-copy
  mutant one on `mutant/...-forward` (both local branches).
- The writer count lives in the sealer as a relaxed atomic: `log_due` reads it per apply, the
  index changes it only at a group's first join after a checkpoint, at the checkpoint and at detach.
