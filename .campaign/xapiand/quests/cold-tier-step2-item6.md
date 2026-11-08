# cold-tier-step2-item6: S3-FIFO catalog eviction policy

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:8bb648d (2026-11-03); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item6`.

## Outcome

The catalog uses frequency-aware small and main FIFOs plus a bounded ghost set to decide demotion and close. A policy pass evicts only over `soft_open`, requeues ineligible candidates and leaves pinned or transitioning slots intact.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S8.1 Hit ratio within 0.5 point of reference at theta 0.99 and 1.2, with/without scan and in both windows: `docs/status/cold-tier-step2.md`
- A2 [artifact] S8.2 Trace: 1M keys, seeded Zipf, 3M warm-up and 8M measured; scan and capacity as specified: `docs/status/cold-tier-step2.md`
- A3 [artifact] S8.3 Stub states run under deferred IO; transition counters are each at least 1: `docs/status/cold-tier-step2.md`
- A4 [artifact] S8.4 Real limits report overshoot and hit-ratio difference: `docs/status/cold-tier-step2.md`
- A5 [artifact] S8.5 Ghost, frequency, every-pin-use and open-only mutants are distinguished by specified windows: `docs/status/cold-tier-step2.md`
- A6 [artifact] S8.6 Each request-pin row sets use frequency; chore pin, handle re-pin and miss pin do not: `docs/status/cold-tier-step2.md`
- A7 [artifact] S8.7 S3-FIFO queue, ghost, frequency, demotion, requeue and close choices are tested: `docs/status/cold-tier-step2.md`
- A8 [artifact] S8.8 Threaded policy is TSan- and ASan+UBSan-clean; queue and pin invariants hold: `docs/status/cold-tier-step2.md`
- A9 [artifact] S8.9 Report CPU per request and pin cost under quiet strict: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Policy is total-based as D6 specifies; the per-FIFO alternative closes below soft cap and is rejected.
- Ghost holds hashes closed from the small FIFO with cleared frequency, not every close.
- Ghost capacity is half of `soft_open`; demotion scan remains limited to 256 slots.
- Risk high: Adds concurrent policy queues, ghost tracking and eviction decisions (the lead's mapping).

## May land early

None.
