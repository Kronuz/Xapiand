# turn-log-parity--checkpoint-window: journal

State: waiting on jobs 261120-203524-f04edf 261120-203525-596222 261120-203526-571372 261120-203528-39f83c 261120-203529-f27b3b 261120-203531-139852 261120-203532-aa7a48 261120-203534-aebc54; blocked on lead: the writer count needs include/strata/core/turn_seal.hh (step 2)
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Floor table: eight exclusive jobs queued at strata:14b476d1, interleaved, two trials of triggers
64 MiB (today's default), 4 MiB, 1 MiB, 256 KiB (ids in State). Each: checkpoint-floor.sh, 10k
logged indexes created under nosync.c then reopened, 8M puts of 100 B, Zipf 0.99, 4 threads.

## Next

1. Read the eight jobs; fill the floor table below; pick the floor (default 1 MiB).
2. When unblocked: the window in `index.hh` (and `turn_seal.hh`), the floor as the option's
   default; register `test_checkpoint_window` in default mode; the window mutant (`log_due` at
   `command::max_bytes`) on `mutant/turn-log-parity--checkpoint-window-window`, `--fails-without`;
   one probe run at the mechanism commit with the chosen floor, to show it equals the fixed row.

## Blocked on

Lead: D2 item 2 divides the window among the groups writing on the log since their checkpoint.
That count, and the window ((retain_segments - 1) x segment_bytes, from the log's geometry), are
per-log state; `index.hh` cannot see either. The sealer (`turn_seal.hh`) is the per-log object and
holds the geometry at construction, but it is outside this task's file set (an edit was refused).
Options:
1. Add `include/strata/core/turn_seal.hh` to the task: the sealer keeps `window_` from its geometry
   and an atomic writer count; `index.hh` counts its group in at its first join after a checkpoint,
   out at the checkpoint and at detach; `log_due` is `logged_bytes >= max(floor, window / writers)`.
   About 10 lines in the sealer. Recommended.
2. Same, but the sealer exposes the log's bytes written instead, and an index checkpoints once the
   log advanced a window since its checkpoint and it logged at least the floor: exact under skew
   (one hot group among 10k cold checkpoints every window, not every floor). A design change to D2.
3. `index.hh` only: a new option, a per-log `checkpoint_window` object every host builds from its
   geometry and passes beside `log` (migrates kv_strata, the tests, Xapiand). More callers, same state.

## Evidence

| Criterion | Evidence |
|---|---|
| A6 (D2's test) | 261120-202804-e233bb: explicit mode passes at strata:14b476d1, at most 5 segment files (bound 6) over segments 1..41, 24 records forwarded (115 KB) for 39.9 MB of commands, 12 writer checkpoints at a 3 MiB window |
| A6, fails without the forward copy | 261120-203457-3324d7 at strata:71d3a338 (`mutant/turn-log-parity--checkpoint-window-forward`, journal's `forward_slice` patched to return nothing): 25 segment files, 0 forwarded, exit 1 |
| A6, fails without the window | 261120-202805-29d178: default mode at today's trigger (`command::max_bytes`, the window mutant's behavior): 7 segment files (bound 6), 30.0 MB forwarded for 5.1 MB of commands, 0 writer checkpoints, exit 1 |

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
- The test registers its explicit mode only (`checkpoint_window_explicit`): the default mode
  fails until D2 item 2 lands.
