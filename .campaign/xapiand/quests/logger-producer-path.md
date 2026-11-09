# logger-producer-path: logger producer path: one L_INFO under 100 ns

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at logger:d0dd832 (2026-11-01); its ledger `docs/status/producer-path.md` in Kronuz/logger is the record. The quest's repository is `../logger`. Its machine state is in `state.json`: `campaign status logger-producer-path`.

## Outcome

One `L_INFO` under steady load costs the producer 73.5 to 77 ns on the M1 Pro (469 to 477 before): the scheduler's `post()` hands a task over with one compare-exchange and the logger formats per thread with a derived wall-clock stamp, output bytes unchanged.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1: one `L_INFO` at most 100 ns on arm64 and the x86 laptop: `docs/status/producer-path.md`
- A2 [artifact] 2: output bytes, ordering, `finish()` drain and the inline path unchanged (differential test against a frozen copy, ordering oracle, TSan, Linux VM stress): `docs/status/producer-path.md`
- A3 [artifact] 3: scheduler and logger landed with CI green; the logger's pin names the scheduler commit: `docs/status/producer-path.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Behavior differences for the owner: the derived stamp (within 2 us), a refusal while filing does not reach the poster, `age()` prefers `cleaned_at`, cross-thread due order by the clock-read gap.
- Risk normal: ordering and loss of log lines across threads (the auditor's mapping).

## May land early

None.
