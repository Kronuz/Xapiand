# journal-seal-repeat: A turn takes several records from one sealer

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status journal-seal-repeat`.

## Outcome

`TurnDriver::seal_turn` calls a group's sealer again with the room left (`first = false`) until it
returns nothing or the room is spent, so one turn carries several records from one group. strata's
ack=none backlog then drains as fast as barriers allow (document-gate durability F1, D107/D108).
Detent's lead accepted the contract change (11-17); only strata calls `TurnDriver::seal`.

## Criteria

- A1 Several records per turn: tests that check values for three records from one sealer in one
  turn with sequences staged+1, +2, +3; the room exactly met, and exceeded by one byte (the record
  goes to the next turn); each record's framing charged against the room before the next call.
  Mutants as `--fails-without` jobs: one call per group; framing not charged; sequence not advanced.
- A2 `first` keeps its meaning: true only for the turn's first record across groups; a repeat call
  passes false. Fairness: `next_seal_` rotation still starts the next turn at the next group (a
  two-group test); any per-group cap is explicit. Mutant: rotation not advanced.
- A3 Failure and lifetime: a sealer throwing on a repeat call leaves its earlier records in the
  turn, as separate turns would (test); a group unsealed or forgotten between repeat calls is not
  called again, and `running_` covers the loop (test under TSan).
- A4 The existing suite, TSan and the crash fixtures pass; CI green on every platform.
- A5 [artifact] The contract is stated in journal's docs: `docs/` describes repeat calls, `first`,
  room accounting and fairness.

## Contracts

Changes `journal:driver` (Sealer is called repeatedly per turn). Consumers: strata (document-gate's
turn log); tell Detent's lead when it lands so Detent repins.

## May land early

Nothing: it is one contract change.
