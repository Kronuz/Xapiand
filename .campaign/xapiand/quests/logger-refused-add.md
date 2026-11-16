# logger-refused-add: logger never loses a refused line

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at logger:90094b5 (2026-10-20); its ledger `docs/status/fix-refused-add.md` in Kronuz/logger is the record. The quest's repository is `../logger`. Its machine state is in `state.json`: `campaign status logger-refused-add`.

## Outcome

`Logging::add` checks the scheduler's `schedule_result`: on a refusal it undoes its counts and, by the line's kind, writes it inline, keeps a watchdog for its cancel point, or loses only a routine future line with no cancel point.

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] Refusal policy by kind after `finish()` or past the 24 h horizon: `docs/status/fix-refused-add.md`
- A2 [artifact] Tests past the horizon and after `finish()`; `pending_count()` exact across every refusal: `docs/status/fix-refused-add.md`
- A3 [artifact] scheduler pinned to 2cbea95 (`schedule_result`): `docs/status/fix-refused-add.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner's policy by line kind; a routine future line without a cancel point is lost, as before (no caller in Xapiand or Detent).
- Risk normal: lost log lines and watchdog accuracy (the auditor's mapping).

## May land early

None.
