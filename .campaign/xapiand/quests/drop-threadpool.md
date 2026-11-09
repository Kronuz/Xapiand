# drop-threadpool: Retire ThreadPool dependency

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at strata:864ea69 (2026-11-02); its ledger `docs/status/drop-threadpool.md` is the record. Its machine state is in `state.json`: `campaign status drop-threadpool`.

## Outcome

Strata pins threadpool's new head and uses WorkPool/WorkGroup only. No source file names `ThreadPool`, `TaskGroup`, or `ThreadPolicyType`.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] No explicit criteria; from the Done list: move the threadpool pin to the new head: `docs/status/drop-threadpool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Task pin moves from 8cad661 to b406766; strata names neither affected API.
- Keep historical `TaskGroup` mentions in design documentation.
- Risk normal: Replaces internal executor usage and dependency pin (the lead's mapping).

## May land early

None.
