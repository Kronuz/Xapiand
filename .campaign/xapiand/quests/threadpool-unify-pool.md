# threadpool-unify-pool: One pool: WorkPool only, ThreadPool family deleted

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at threadpool:3f1fbcf (2026-11-02); its ledger `docs/status/unify-pool.md` in Kronuz/threadpool is the record. The quest's repository is `../threadpool`. Its machine state is in `state.json`: `campaign status threadpool-unify-pool`.

## Outcome

`WorkPool` is the Kronuz family's only pool: `WorkGroup::pending()` and `running()` added (step 1), the scheduler library moved onto it (step 2), and `ThreadPool`, `TaskGroup`, `PackagedTask` and their family deleted (step 3), with consumers repinned in one wave and a table of `WorkPool` equivalents for Detent.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Step 1: `WorkGroup::pending()`/`running()` with `test/work_counts.cc`; both counter mutants fail it: `docs/status/unify-pool.md`
- A2 [artifact] Step 3: the ThreadPool family, its tests, bench and research harness deleted; `ThreadPolicyType` trimmed to `regular` and `logging`: `docs/status/unify-pool.md`
- A3 [artifact] Proof: ctest 6/6; preflight mac hardened, mac ASan+UBSan, Linux and FreeBSD ok at db38594; CI run 41546508647 green on 8 jobs: `docs/status/unify-pool.md`
- A4 [artifact] Repins on each consumer's `phase7c/drop-threadpool`, CI green: scheduler 1e5ccc9, logger 2356791, strata 3f564dc, prospect bd78224: `docs/status/unify-pool.md`
- A5 [artifact] Xapiand on `deps/drop-threadpool`: Release and ASan+UBSan ctest 32/32, `async_check --strict` 0, docs E2E 641/641 (Xapiand:602121ecd, 45d81215e; local only at the time): `docs/status/unify-pool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Removing the `ThreadPolicyType` parameter itself is a follow-up the owner has not ruled.
- Detent stays on its pinned threadpool and migrates itself; the ledger's "Note for the Detent lead" lists the `WorkPool` equivalents (Detent's `pin-wave` inherits it).
- Risk high: removes a public pool API every consumer used (the auditor's mapping).

## May land early

None.
