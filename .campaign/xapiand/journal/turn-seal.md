# turn-seal: journal

State: working
Quest: .campaign/xapiand/quests/turn-seal.md (the lead's; never edit it)

## Doing

journal:b08212f (pushed): build-test 261117-020559-d9967e (40/40) and tsan 261117-020559-c74944
green; mutants killed, `--fails-without`: A2 261117-020600-2e3287, A7 261117-020603-a0abbc. The
jobs at journal:4e0e523 failed on a test build error (`room` shadowed); fixed in journal:01387e7,
along with the budget case's `firsts` and the sequence mutant's anchor. Next: tagged A1/A3/A8 jobs,
then ready for review.

## Next

1. When green: tagged jobs A1 `crash`, A2 `sequence`, A3 `budget full_size bound`, A7
   `supersede`, A8 `unseal_owner lock_order wake_threads` (the wake under TSan rides the tsan check).
2. Fill Evidence, push the branch, set ready for review.

## Blocked on

R8 made `covers` mandatory, so checkpoint callers outside the file set needed one field each:
`test/turn_log_test.cc`, `test/turn_log_power_test.cc` (their `record()` helpers),
`test/placement_test.cc` (one line), `fuzz/fuzz_turn_log.cc`, `bench/turn_bench.cc`. I changed them
(journal:4e0e523); the set needs them. Every caller can know `covers`, none is blocked.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | mutant 261117-020600-2e3287 (`sequence` fails without the check); plain case in build-test 261117-020559-d9967e |
| A3 | pending |
| A4 | tsan 261117-020559-c74944 (`forget_*`) |
| A5 | `docs/mechanisms/turn-seal.md`, `CHANGELOG.md` 2026-11-16 |
| A6 | build-test 261117-020559-d9967e, 40/40 |
| A7 | mutant 261117-020603-a0abbc (`supersede` fails without the guard); plain case in build-test 261117-020559-d9967e |
| A8 | pending |

## Result

`TurnDriver::seal(group, fn)` with `using Sealer = std::function<std::optional<TurnRecord>(std::uint64_t budget)>`,
in `include/journal/turn_driver.h`; the turn bound in `turn_log.h`; tests `test/turn_seal_test.cc`;
contract `docs/mechanisms/turn-seal.md`.

## Rulings I made

- R1 Refusal is fail-stop: a sealed record of another group, out of sequence, past its budget, or
  from a sealer that forgot its group fails the turn like a log refusal. The sealer has already
  handed over its record, so refusing without failing would leave its `durable()` waiter to be
  satisfied by whatever later took the sequence (a false acknowledgement).
- R2 No start exemption for sealed records: a sealer continues its group (expected = last staged +
  1, so 1 for a group the log lacks); a group starts through `stage()`. This is also what makes the
  A2 mutant observable: the log alone admits a checkpoint as a group's start at any sequence.
- R3 Budget unit is payload bytes, from a manifest-proof bound on encoded bytes (the doc derives
  it). The first sealed record of a turn gets `turn_room(segment_bytes)` minus pending, later ones
  share `turn_room(segment_bytes / 2)`; the hard invariant is "fits an empty segment".
- R4 Rotation: a sealer uncalled (budget spent) or declining a share is called first next turn.
- R5 Turn trigger: a `durable()` waiter above a sealed group's staged sequence makes `idle()`
  false; a sealer answering nothing to the whole budget goes quiet until a waiter links again. No
  new API; without it a consumer with only an open record never gets a turn under `run()`.
- R6 Loop-confined like the rest of the driver: `seal`, `forget` and sealers run on the loop; A4's
  TSan case is the consumer's shape (a writer thread under the consumer's lock).
- R8 (lead's ruling applied) `covers` is a plain field, default 0 = covers nothing; `TurnLog::stage`
  refuses a checkpoint with `covers` below the record before it (durable or earlier in the same
  turn, since that record is acknowledged at the barrier that drops it), so only a group's first
  record may leave it 0. Callers: Journal publication `covers = record_sequence_` (its check
  `covered_sequence == frontier_.sequence` and no pending mutation make every batch record covered),
  create 0, reopen `entry->base`; Host re-partition restages only each group's first kept record
  (a start); Store stages none.
- R9 A8 matched to strata:efe25bcd's shim where sound: `seal(group, fn, owner)` replaces and
  returns the last staged sequence; `wake()` any thread; `fn(room, first)` in `record_cost` units.
  Per the lead's ruling `unseal(group, owner)` is any-thread and blocks like a timer cancel until
  the call in progress finishes (a separate table mutex, never held across a call; from inside
  the sealer it returns at once). A record that call returns is staged as the group's. Not
  matched: the shim's watcher, `await`, `serve`/`stop`, several records per group per turn.
- R7 `commit()` with nothing pending calls the sealers and throws "no records to write" only when
  nothing was pending, owed, or sealed. `stage()` from inside a sealer is refused.
