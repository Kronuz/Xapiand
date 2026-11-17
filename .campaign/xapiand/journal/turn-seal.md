# turn-seal: journal

State: working
Quest: .campaign/xapiand/quests/turn-seal.md (the lead's; never edit it)

## Doing

Dev job 261116-211639-f3740c (build + `turn_seal|driver` tests) running; fix what it shows, then
the evidence jobs.

## Next

1. Green dev job, then `campaign verify turn-seal` (build-test, tsan) and per-criterion jobs:
   A1 `crash`, A2 `sequence` + mutant (`--fails-without`, own slot), A3 `budget full_size bound`,
   A4 `forget_*` under TSan.
2. Fill Evidence, push the branch, set ready for review.

## Blocked on

The file set lacks `CMakeLists.txt`: the new test needs one word in its `foreach` and its CTest
entries. Options: (a) the lead adds `CMakeLists.txt` to the file set (recommended: test
registration only); (b) fold the cases into `driver_test.cc`, also outside the set. I made (a)'s
change; landing needs the set amended.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | `docs/mechanisms/turn-seal.md`, `CHANGELOG.md` 2026-11-16 |
| A6 | pending |

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
- R7 `commit()` with nothing pending calls the sealers and throws "no records to write" only when
  nothing was pending, owed, or sealed. `stage()` from inside a sealer is refused.
