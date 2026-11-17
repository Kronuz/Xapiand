# turn-seal: journal

State: waiting on jobs 261116-213841-f1e2c2 261116-213842-954d55 261116-213905-1bce9a 261116-213908-a0987f
Quest: .campaign/xapiand/quests/turn-seal.md (the lead's; never edit it)

## Doing

At journal:ba827e1: build-test 261116-213841-f1e2c2, tsan 261116-213842-954d55, mutants A2
261116-213905-1bce9a (`sequence`) and A7 261116-213908-a0987f (`supersede`), `--fails-without`, own
slots. Dev job 261116-211933-50e55d (journal:e61906e) found three test bugs (bound at zero room, a
temporary Sentinel, forget_threads never idle for group 1), fixed in journal:fe9025f. Lead's A7
and A8 messages folded in (journal:e61906e, journal:fe9025f).

## Next

1. When green: tagged jobs A1 `crash`, A2 `sequence`, A3 `budget full_size bound`, A7
   `supersede`, A8 `unseal_owner lock_order wake_threads` (the wake under TSan rides the tsan check).
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
| A7 | pending |
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
- R8 A7: the log cannot read a checkpoint's payload, so `TurnRecord::covers` (optional, not stored,
  not compared) carries its covered sequence; `TurnLog::stage` refuses `covers` below the record
  before it. Unset keeps every caller (Journal, Host, fuzz, bench, tests) unchanged, so the guard
  bites only callers that state coverage; strata standalone must set it. Making it mandatory would
  touch `journal.h` (outside the file set): say if you want that.
- R9 A8 matched to strata:efe25bcd's shim where sound: `seal(group, fn, owner)` replaces and
  returns the last staged sequence (the shim returns last durable; staged is what the next record
  follows); `unseal(group, owner)`; `wake()` any thread; `fn(room, first)` with room in
  `record_cost` units (strata's `turn_framed_bytes` is stricter, so its check stays safe). Not
  matched: the shim's watcher, `await`, `serve`/`stop` (strata uses `durable()` and `run()` per D4),
  its several records per group per turn (the quest says one), and its any-thread `unseal`, whose
  "a turn in progress may still call them" would let a destroyed index's sealer run; mine is
  loop-confined and no call starts after it returns. strata must call `unseal` on the loop.
- R7 `commit()` with nothing pending calls the sealers and throws "no records to write" only when
  nothing was pending, owed, or sealed. `stage()` from inside a sealer is refused.
