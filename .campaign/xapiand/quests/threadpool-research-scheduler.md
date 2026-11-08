# threadpool-research-scheduler: Scheduler research note

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at threadpool:394aaea (2026-10-18); its ledger `docs/status/research-scheduler.md` in Kronuz/threadpool is the record. The quest's repository is `../threadpool`. Its machine state is in `state.json`: `campaign status threadpool-research-scheduler`.

## Outcome

threadpool carries `docs/research/scheduler.md`: the per-task and wake-latency bound on the Mac and the gate VM, a survey of production schedulers, five prototypes under `research/` measured on one hot group, 1,000 busy groups and 10,000 idle ones, and the three recommendations (R1 to R3) L2 built.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 the bound: queue ops 12 to 85 ns, a wake 10 us p50 on the Mac; the VM is not a latency floor: `docs/status/research-scheduler.md`
- A2 [artifact] K2 the survey with sources (Chase-Lev, Tokio, Go, oneTBB, Taskflow, folly, Rayon, Seastar, io_uring, DRR): `docs/status/research-scheduler.md`
- A3 [artifact] K3 at least three designs prototyped under `research/` against today's pool, Taskflow and moodycamel; TSan clean: `docs/status/research-scheduler.md`
- A4 [artifact] K4 the workloads measured: tiny tasks, query p50/p99/p99.9 under saturation, makespan, idle CPU, memory per group: `docs/status/research-scheduler.md`
- A5 [artifact] K5 `docs/research/scheduler.md` with the top 3 recommendations, an additive API sketch and what strata and reactor change: `docs/status/research-scheduler.md`
- A6 [artifact] K6 CI green on all 8 jobs (run 39137948331 at d112fde): `docs/status/research-scheduler.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The research tree is off by default (`THREADPOOL_BUILD_RESEARCH=ON`); `research/` was later deleted with the ThreadPool family (unify-pool step 3) and the note marked HISTORY.
- Risk low: a research note and an off-by-default prototype tree (the auditor's mapping).

## May land early

None.
