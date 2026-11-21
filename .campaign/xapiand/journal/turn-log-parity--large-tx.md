# turn-log-parity--large-tx: journal

State: waiting on jobs 261121-104119-d0dd11 261121-104122-87c06b 261121-104124-6f93ff 261121-104127-764b43
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 pushed (strata:e8e12a8c on origin). Jobs at the task head:

| Job | What |
|---|---|
| 261121-104110-491873 | A3: test_large_tx, every case |
| 261121-104113-4f8cbb | mutant limit (strata:dc3ca2e60bb9): admit must fail |
| 261121-104116-816fc6 | mutant per-chunk (strata:cb5fa6cda2cc): sweep must fail |
| 261121-104119-d0dd11 | mutant early-v (strata:d4f7165bb272): visible must fail |
| 261121-104122-87c06b | mutant keep-applied (strata:f686420039da): sweep must fail |
| 261121-104124-6f93ff | A3 fuzz: fuzz_command with the chunk kind, 2 x 10 min |
| 261121-104127-764b43 | the suite at the task head |

Locally (targeted, under light holds): codec, t8, t9o, kv_strata_batch_test, the fuzz corpus
replay, admit (66 MiB in 67 chunk records) and visible pass; sweep and big were not timed locally
(the host was full), so the A3 job sizes them.

Results so far:

- 261121-104110-491873 passed: admit (66 MiB in 67 chunk records; both refuse past the unlogged
  cap); sweep 558 cases over 61 mutations (26 records), 531 runs cut, all whole prefixes; big 147
  cases at 49 of 503 mutations (74 records), 105 runs cut; visible 0 exposed early.
- 261121-104113-4f8cbb (limit) failed as required: admit refuses the 66 MiB transaction.
- 261121-104116-816fc6 (per-chunk) failed as required: "no whole prefix matches" from cut 8.

## Next

1. Read the four pending jobs; a failure is fixed with its test, as a job.
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
| A3 | 261121-104110-491873 (test_large_tx, passed); mutants 261121-104113-4f8cbb (limit), 261121-104116-816fc6 (per-chunk) failed as required; pending: 261121-104119-d0dd11 (early-v), 261121-104122-87c06b (keep-applied), 261121-104124-6f93ff (fuzz_command with kind 3), 261121-104127-764b43 (suite) |

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
