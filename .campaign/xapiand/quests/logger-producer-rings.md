# logger-producer-rings: logger per-thread queues (logger#1)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at logger:765528e (2026-11-01); its ledger `docs/status/producer-rings.md` in Kronuz/logger is the record. The quest's repository is `../logger`. Its machine state is in `state.json`: `campaign status logger-producer-rings`.

## Outcome

A routine line costs its thread no lock, allocation or read-modify-write: per-thread rings with an 8-byte wrap record, the LOG thread sleeping until the earliest due line, every guarantee kept.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1: one producer pays at most a synchronous fmt + mutex logger on the M4 Pro and the i9; four producers clearly less: `docs/status/producer-rings.md`
- A2 [artifact] 2: every guarantee kept (per-thread order, ordered posts, delayed and cancel, `clears`, isolated-line latency, `max_pending`, flush, handlers, API): `docs/status/producer-rings.md`
- A3 [artifact] 3: TSan-clean stress at 1 to 16 producers that fails with the fix reverted; ring mutants caught; ASan/UBSan: `docs/status/producer-rings.md`
- A4 [artifact] 4: one `quiet strict` hold per machine, START against main against NEW with A/A; README and CHANGELOG; CI run 41396610451 green: `docs/status/producer-rings.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The wrap record is an 8-byte `RecordHead` written with memcpy (the root cause of the prototype's heap corruption).
- Risk high: lock-free rings and heap layout across threads (the auditor's mapping).

## May land early

None.
