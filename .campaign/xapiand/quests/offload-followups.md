# offload-followups: Bound control-worker command draining

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:201414347 (2026-10-26); its ledger `docs/status/offload-followups.md` is the record. Its machine state is in `state.json`: `campaign status offload-followups`.

## Outcome

DB_UPDATED refreshes sender liveness on the bus loop before dispatch. Control-worker jobs process at most 64 commands before re-queuing, allowing queued work to run.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] CR2a Refresh DB_UPDATED sender liveness at message time; dispatch only a READY node: `docs/status/offload-followups.md`
- A2 [artifact] CR2b Drain at most 64 commands per job, then re-post behind already queued work: `docs/status/offload-followups.md`
- A3 [artifact] CR5 Pin reactor to main `0c04d0d`; remove the "lead repins" comment: `docs/status/offload-followups.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- D13 belongs in the strata repository; this ledger points to the lead's required update.
- Risk normal: Adjusts discovery timing and worker scheduling (the lead's mapping).

## May land early

None.
