# prospect-drop-threadpool: prospect pins strata and task after the pool change

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at prospect:0744e51 (2026-11-02); its ledger `docs/status/drop-threadpool.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-drop-threadpool`.

## Outcome

prospect pins strata 3f564dc (threadpool eb4751a, task b406766) and task b406766, with `lock(token)` replacing `lock_until` and `lock_for`.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Pins moved: strata 501f943 to 3f564dc, task 8cad661 to b406766: `docs/status/drop-threadpool.md`
- A2 [artifact] Proof: preflight mac hardened, mac ASan+UBSan, Linux, FreeBSD ok at bd78224; CI run 41546540162 green on 8 jobs: `docs/status/drop-threadpool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Landed in the wave order threadpool, scheduler, logger, strata, prospect, by fast-forward.
- Risk low: a pin move (the auditor's mapping).

## May land early

None.
