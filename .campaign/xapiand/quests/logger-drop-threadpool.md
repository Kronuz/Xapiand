# logger-drop-threadpool: logger pins the scheduler on WorkPool

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at logger:d63fec3 (2026-11-02); its ledger `docs/status/drop-threadpool.md` in Kronuz/logger is the record. The quest's repository is `../logger`. Its machine state is in `state.json`: `campaign status logger-drop-threadpool`.

## Outcome

logger pins the scheduler head that runs on `WorkPool`; it names only `ThreadPolicyType::logging`, so no source changes.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Pin moved from scheduler 2bb0a8f to 1e5ccc9 (threadpool eb4751a): `docs/status/drop-threadpool.md`
- A2 [artifact] Proof: preflight mac hardened, mac ASan+UBSan and Linux ok; CI run 41546525411 green on all 8 jobs: `docs/status/drop-threadpool.md`
- A3 [artifact] FreeBSD preflight `logger_clock` timing slack on a busy VM: `docs/status/drop-threadpool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- http-log and prism pin logger 801105c and need no change.
- Risk low: a pin move with no source change (the auditor's mapping).

## May land early

None.
