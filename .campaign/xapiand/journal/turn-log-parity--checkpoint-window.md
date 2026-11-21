# turn-log-parity--checkpoint-window: journal

State: blocked on lead: the writer count needs include/strata/core/turn_seal.hh (step 2); steps 1 and 3 proceed
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1, the floor probe: `bench/kv/probes/rocksdb-parity/checkpoint-floor.cc` and `.sh`, 10k logged
indexes on one turn log, Zipf 0.99 writes; trigger set through today's `checkpoint_log_bytes`.

## Next

1. Floor table: smoke (light), then exclusive runs at 64 MiB (today's default), 4 MiB, 1 MiB, 256 KiB.
2. `test/test_checkpoint_window.cc`: D2's test with values, what holds today (writer checkpoint set
   to the window by option); mutant without the forward copy (`--fails-without`).
3. When unblocked: the window mechanism in `index.hh` + `turn_seal.hh`; the test's default case;
   the mutant at `command::max_bytes`.

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

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
