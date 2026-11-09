# scheduler-unify-pool: Scheduler on WorkPool

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at scheduler:1e5ccc9 (2026-11-02); its ledger `docs/status/unify-pool.md` in Kronuz/scheduler is the record. The quest's repository is `../scheduler`. Its machine state is in `state.json`: `campaign status scheduler-unify-pool`.

## Outcome

`ThreadedScheduler` owns a `WorkPool` and one `WorkGroup` instead of a `ThreadPool`; `size()`, `running_size()`, `finish()` and a bounded `join()` keep their meaning, a refused dispatch is counted, and the scheduler pins the threadpool without the ThreadPool family.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Tests: `test_threaded_pool_shape`, `test_finish_drops_due_tasks`; ctest RelWithDebInfo passes: `docs/status/unify-pool.md`
- A2 [artifact] Seven mutants each fail a test (`.scratch/mut.sh`): `docs/status/unify-pool.md`
- A3 [artifact] Bench under `quiet strict`, 6 cycles x 3 binaries with an A/A: `add()` and `L_INFO` within the control, firing 200,000 due tasks 5.7x faster (2522 to 443 ns/task); raw in `docs/status/unify-pool/ab-main.txt`: `docs/status/unify-pool.md`
- A4 [artifact] Sanitizers: preflight on 4 hosts and TSan clean on `scheduler_test` and threadpool's work_ tests: `docs/status/unify-pool.md`
- A5 [artifact] CI on the merged tree: threadpool run 41542646368 and scheduler run 41542683108 green: `docs/status/unify-pool.md`
- A6 [artifact] Review follow-up CR1, CR4, CR6 done (merge of main, API note, `refused_size()`); CR7 to the lead: `docs/status/unify-pool.md`
- A7 [artifact] Step 4: pin threadpool eb4751a on `phase7c/drop-threadpool`: `docs/status/unify-pool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- CR7 (thread policies ignored at run time) went to the lead; not recorded as ruled.
- Risk high: the timer and debounce scheduler Xapiand and logger run on (the auditor's mapping).

## May land early

None.
