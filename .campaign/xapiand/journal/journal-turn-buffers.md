# journal-turn-buffers: journal

State: working
Quest: .campaign/xapiand/quests/journal-turn-buffers.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First cut at journal:b7501df (code, tests, mutants) and journal:a210537 (mechanism notes), pushed.
Focused build job 261120-200225-e0b203 (turn_buffers, framing, turn_log, crc_fixture tests).

## Next

1. Fix what the focused build reports; then evidence jobs: A1 (`bytes`, `turns`, crc_fixture,
   framing), A2 (`warm`, `seal` + `--fails-without` at `seal-copy` and `stage-copy`), A3 (`forward`
   + `--fails-without` at `forward-string`), A4 (`power`, turn_log_power, turn_seal).
2. CI (macos-arm64, asan, tsan) green on the pushed branch.
3. Write "ready for Detent's review" with the commit and tests (A5).

## Blocked on

Nothing blocks A1-A4. One gap for the lead, outside this file set: strata's half wants its body
buffer back for reuse (D3, "one encode buffer per log, reused every turn"). The driver moves the
sealed `TurnRecord` into `pending_`/`flight_` and frees its payload when the turn completes, so a
strata log_state that hands over its body by move allocates a new body each turn. Options:
1. Widen this quest's file set to `turn_driver.h` for a small hook: after `stage()`, hand each
   sealed record's payload back to its sealer (for example a `recycle(std::string&&)` beside
   `Sealer`), cleared, capacity kept. Recommended: it is the last allocation on the sealed path.
2. Leave it to turn-log-parity task 2 to keep two buffers and accept one allocation per turn.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- R1 "Header room" is the consumer's: the emitter takes the 25-byte record header as a separate
  piece beside the body, so `TurnRecord` and every caller (Journal, Store, Host, the driver,
  Detent) stay unchanged; strata reserves room only for its own batch header.
- R2 `framing::Manifest::groups` becomes `framing::Groups`, a flat sorted-on-read array with the
  map's `operator[]`, brace init and ordered iteration, so turn_seal_test compiles unchanged and a
  block's manifest allocates nothing once warm.
- R3 A2's "warm turn" window is `TurnLog::stage()` and `completed()`; the write and barrier are
  the IO seam's (faultfs allocates there). Through the driver (`seal`), `begin()` includes the
  driver's own bookkeeping vectors (outside this file set), so that case bounds bytes allocated
  (under a quarter of one body) rather than counting zero.
- R4 Map nodes freed by the turn path (`next`, the live index) are kept for reuse without a cap:
  never more than the map held at its largest, so steady checkpoint cycles allocate none.
- R5 `Live` keeps the payload size in its padding (`bytes : 56`, sizeof 32 unchanged) so a forward
  copy's layout is known before it is read; the copy now also checks its original's record
  header (kind, group, sequence) and size, as `live()` does.
