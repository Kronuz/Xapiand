# turn-seal: TurnDriver::seal: one sealed record per group per turn

Risk: high (durability, a shared journal API). The lead writes this quest; contributors read it and
never edit it. Its machine state is in `state.json`: `campaign status turn-seal`. Repository:
Kronuz/journal, from journal:7c08540. Detent's lead reviews it.

## Outcome

strata's standalone durability (document-gate A1, design strata:8e919415
`docs/design/standalone-turn-log.md`) needs `TurnDriver::seal(group, fn)`: an additive API, no format
change. `start()` on the loop calls each registered group's `fn(budget)` once, after the turn's
pending records and before encoding; `fn` returns at most one `TurnRecord` whose sequence is the
group's last staged sequence + 1, or nothing. The driver passes the bytes left under
`segment_bytes / 2` and stops calling sealers once that budget is spent; a record not taken stays
open for the next turn; `forget(group)` unregisters the sealer.

## Acceptance criteria

- A1 The sealed record goes in the same turn, under the same sync, as the pending records it
  follows: a crash test (faultfs) at every sync point shows the sealed record durable iff the turn's
  records are, never alone (a test, run as a job).
- A2 A sealed record's sequence is checked against the group's last staged sequence + 1 and refused
  otherwise; a test shows a checkpoint can never be staged behind an acknowledged record (the
  document-gate design review's F1/P1 shape), with a mutant that drops the check failing it
  (`--fails-without`).
- A3 A sealer refused for budget is called again on the next turn and its record is never dropped;
  the budget always admits a turn's first record, so a full-size record seals (review R3): tests.
- A4 `forget(group)` unregisters the sealer while a turn is open, and no sealer runs after it
  returns (a test, under TSan: the `tsan` check).
- A7 `TurnLog::append` refuses a checkpoint record whose covered sequence is below the group's last
  acknowledged record, at the lowest layer, so Journal, Host and strata standalone share one guard:
  the document-gate design review's supersede probe (P1, Xapiand `.scratch/review-document-gate-design/
  supersede.cc`) as a test, and a mutant without the guard failing it (`--fails-without`)
- A8 The contract strata's shim needs (strata:efe25bcd `include/strata/core/turn_seal.hh`, design
  "Built"): `unseal` keyed by its owner, so a reopened index's registration survives the old one's
  destructor; a thread-safe wake when an open record takes its first transaction, so `run()` and
  `journal::run` see unstaged work; the turn budget's room passed to the sealer; and the lock order
  turn mutex, then group record mutex, never the reverse: tests, the wake under TSan
- A5 [artifact] `docs/mechanisms/turn-seal.md` states the contract, and CHANGELOG.md the addition.
- A6 The suite passes (the `build-test` check).

## Contracts

Provides `journal:turn-seal` (API). Consumes `journal:turn-log` (RFTW0001, unchanged).

## Decisions

- Detent's lead (2026-11-16): the supersede guard goes in TurnLog here (A7); consensus's Worker is
  already safe (consensus:41ee584, `checkpoint_keeps_acknowledged`).

- Detent's lead (2026-11-16): build it as a xapiand journal quest; Detent reviews. Shape notes from
  Detent are criteria A1-A4.

## May land early

None.
