# turn-seal: journal

State: waiting on jobs (fix round 1 at journal:7549bf6, pushed)
Quest: .campaign/xapiand/quests/turn-seal.md (the lead's; never edit it)

## Doing

Fix round 1 for the durability review (.campaign/xapiand/reviews/turn-seal-durability.md,
CHANGES at journal:b08212f), F1-F3 in journal:7549bf6; N1-N5 left to the lead.

- F1 (`turn_driver.h` seal_turn): a declining sealer goes quiet only when `room == whole` (first,
  nothing pending); otherwise `resume_at`. Test `staged_room` (blocking entry and run()).
  Fails without: 261117-022313-4f355d; passes: A3 261117-022227-105658.
- F2 (`wake`, `woken`, run()'s `Loop`): a wake posts a refcounted `Wakeup` that `~Loop` detaches,
  so a wake queued past run() reaches no driver. Test `wake_after_run` (ASan). Fails without
  (ASan, pre-fix header): 261117-022322-bc38d9; passes: ASan 261117-022301-f1ed5e.
- F3 (`unseal`): waits on the running call's (group, owner), recorded beside `running_`. Test:
  both probe shapes in `unseal_threads`. Fails without: 261117-022317-0065a2; passes: A8
  261117-022243-b05cb2, tsan 261117-022210-100fa8.
- Reviewer's probes at the fix (exit 0 = all hold): 261117-022218-bcb057.
- Gate: build-test 261117-022210-60ccdf, tsan 261117-022210-100fa8.

"Fails without" jobs build the fix commit with `include/journal/turn_driver.h` checked out at
journal:b08212f: the new tests against the old driver.

## Next

1. When the jobs finish: fix any red, else ready for the durability reviewer's re-check.

## Blocked on

R8 made `covers` mandatory, so checkpoint callers outside the file set needed one field each:
`test/turn_log_test.cc`, `test/turn_log_power_test.cc` (their `record()` helpers),
`test/placement_test.cc` (one line), `fuzz/fuzz_turn_log.cc`, `bench/turn_bench.cc`. I changed them
(journal:4e0e523); the set needs them. Every caller can know `covers`, none is blocked.

## Evidence

At journal:7549bf6 (fix round 1):

| Criterion | Evidence |
|---|---|
| A1 | 261117-022222-ec9746 (`crash`); build-test 261117-022210-60ccdf |
| A2 | 261117-022232-4ebd89 (`sequence`); mutant 261117-022305-03393d (`--fails-without`) |
| A3 | 261117-022227-105658 (`budget full_size staged_room bound`); F1 fails-without 261117-022313-4f355d |
| A4 | tsan 261117-022210-100fa8 (`forget_*`) |
| A5 | `docs/mechanisms/turn-seal.md`, `CHANGELOG.md` 2026-11-16 |
| A6 | build-test 261117-022210-60ccdf |
| A7 | 261117-022238-349505 (`supersede`); mutant 261117-022309-097d1d (`--fails-without`) |
| A8 | 261117-022243-b05cb2 (`unseal_owner lock_order wake_threads wake_after_run unseal_threads`); ASan 261117-022301-f1ed5e; tsan 261117-022210-100fa8; F2 fails-without 261117-022322-bc38d9; F3 fails-without 261117-022317-0065a2 |
| Review F1-F3 | probes 261117-022218-bcb057 |

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
- R10 (review F2) A wake queued when run() returns is dropped, not drained: its `woken_` flag
  stays set, so the next run() or blocking pass takes a turn for it. Detaching covers run()'s
  every exit (return, throw, frame destroyed), which draining before `co_return` would not.
