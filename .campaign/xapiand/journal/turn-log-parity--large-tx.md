# turn-log-parity--large-tx: journal

State: waiting on jobs 261121-110105-830c31 261121-110107-c64ff6 261121-110108-aa171b 261121-110110-5b9806 261121-110112-a647c1 261121-110113-052233 261121-104124-6f93ff
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 2 pushed (strata:39b28f25): the lead's finding that the keep-applied mutant survived
(261121-104122-87c06b exited 0, so the sweep passed with the mutation). It was a gap in the test,
not an equivalent mutant. The sweep's second life wrote before it committed, so the new records'
position hid an applied position left behind a dropped run. With the mutant, a checkpoint that
comes with no write after recovery throws: a close's leave stages one with x at the head and
`next` past the dropped run. The sweep now closes right after the first recovery and reopens,
checking the state again by value. Locally under light holds, with one seed: the fix passes (124
cases); the mutant fails from cut 18: "recovery or its close threw: strata: a checkpoint record
with batches not yet durable".

Jobs at strata:39b28f25 (mutants rebuilt on it):

| Job | What |
|---|---|
| 261121-110105-830c31 | A3: test_large_tx, every case |
| 261121-110107-c64ff6 | mutant limit (strata:32759353de29): admit must fail |
| 261121-110108-aa171b | mutant per-chunk (strata:1af1de02b9ba): sweep must fail |
| 261121-110110-5b9806 | mutant early-v (strata:a09553de2476): visible must fail |
| 261121-110112-a647c1 | mutant keep-applied (strata:c4d7a01a3da8): sweep must fail |
| 261121-110113-052233 | the suite at the task head |
| 261121-104124-6f93ff | A3 fuzz at strata:e8e12a8c (still running); command.hh and fuzz/ are unchanged in strata:39b28f25 |

Round 1 at strata:e8e12a8c, for the record:

- 261121-104110-491873 passed: admit (66 MiB in 67 chunk records); sweep 558 cases over 61
  mutations; big 147 cases at 49 of 503 mutations; visible 0 exposed.
- Mutants limit (261121-104113-4f8cbb), per-chunk (261121-104116-816fc6) and early-v
  (261121-104119-d0dd11: "exposed early: 1") failed as required. keep-applied
  (261121-104122-87c06b) survived: the gap above.
- 261121-104127-764b43 (suite at strata:e8e12a8c) was cancelled for the head's run.

## Next

1. Read each job; a failure is fixed with its test, as a job.
2. Fill Evidence from `campaign brief`, write Result, State "ready to merge".

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
| A3 | at strata:39b28f25: 261121-110105-830c31 (test_large_tx); mutants 261121-110107-c64ff6 (limit), 261121-110108-aa171b (per-chunk), 261121-110110-5b9806 (early-v), 261121-110112-a647c1 (keep-applied), all --fails-without; fuzz 261121-104124-6f93ff (kind 3, at strata:e8e12a8c, same codec); all pending |

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
