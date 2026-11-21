# turn-log-parity--large-tx: journal

State: working
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code round 1 committed (strata:e8e12a8c); timing the sweeps locally to size the jobs. Mutant
commits on local branches `mutant/large-tx/*`: limit (strata:dc3ca2e60bb9), per-chunk
(strata:cb5fa6cda2cc), early-v (strata:d4f7165bb272), keep-applied (strata:f686420039da).

## Next

1. Submit jobs: the suite at the head, test_large_tx (all cases), the fuzzer over the command
   corpus, and one `--fails-without` job per mutant.
2. Push the branch once; wait once; record ids.

## Blocked on

Nothing. msg-202 answered: the lead added the eight paths to the quest's set (the task's own set
cannot widen; `campaign check` warns "outside its scope", expected).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- A chunk is its own payload kind (3: place byte, then the slice), one per record, rather than
  rocksdb-parity A3's per-entry part byte in a batch version 2. Batch bytes and fixtures stay
  unchanged; a run costs at most two more records than packing would, against T / command_bytes
  records; replay decides by record kind.
- `command_bytes` keeps its name and becomes the record cap: chunks are `command_bytes - 3` bytes,
  so the sealer's segment check (two framed commands) still bounds every record.
- A run interrupted by a later record or the log's end is dropped and moves the applied position
  to its last chunk, as a rejected transaction moves it (SR-D16): the next checkpoint then covers
  the run's records. Without that move, the checkpoint's `x.index + 1 == next` rule throws.
- The command is encoded whole before stage 1, as before; the streaming encoder in A3 item 6
  (memory T plus one room) is not built: memory is the encoding beside the applied form.
- The design's "leave the freeze's record open" mutant has no target: D4's background flush
  (parity task 6) is not built, and the lease already keeps flushes out of a run.
- test_large_tx.cc copies host_view from standalone_turn_log_test.cc, as test_ryw.cc does.
- Functions changed in index.hh: `apply` (the refusal removed), `log_state::join_chunk` (new),
  `join_log` (calls `join_chunks`), `join_chunks` and `await_sealed` (new), `replay_log` and
  `replay_one` (split out). group-commit's `joined()` calls go in `join_chunks` too at merge.
