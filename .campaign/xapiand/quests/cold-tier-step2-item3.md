# cold-tier-step2-item3: Catalog slots pins and word protocol

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:be5c5af (2026-11-02); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item3`.

## Outcome

`catalog` manages sharded slots, lock-free pin-word transitions, open single-flighting, waiter delivery and retries. The word's state and generation track slot lifecycle, and a frequency bit records request-pin use.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S4 T6 schedules cover word writers, slot reuse, reopen and compare-exchange windows: `docs/status/cold-tier-step2.md`
- A2 [artifact] S4 Transition pins suspend and receive pins or errors from the transition's store: `docs/status/cold-tier-step2.md`
- A3 [artifact] S4 Pin release during a transition and retry into a new transition are not lost: `docs/status/cold-tier-step2.md`
- A4 [artifact] S4 Waiters resume outside the mutex; `try_pin` handles open and wake starts: `docs/status/cold-tier-step2.md`
- A5 [artifact] S4 Re-pin across reopen falls back to key; failed opens back off: `docs/status/cold-tier-step2.md`
- A6 [artifact] S4 Conflicting keys on held directories fail once; use bit, unpin ordering and cold-close rules hold: `docs/status/cold-tier-step2.md`
- A7 [artifact] S4 Stress runs under TSan; sweeps read fields only after compare-exchange: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The index type is templated; slots are allocated in 256-slot chunks, maximum 4M slots.
- Request waiters joining demoting/closing can be handed to waking; chore waiters stay waiting through wake.
- Failed-open and retry each bump generation; `try_pin` miss inserts an opening slot.
- Risk high: Introduces concurrent slot transitions, pin accounting and waiter delivery (the lead's mapping).

## May land early

None.
