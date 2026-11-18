# hybrid-go-no-go: journal

State: waiting on jobs 261118-000128-f4c57b and the C1 chain (Doing)
Quest: .campaign/xapiand/quests/hybrid-go-no-go.md (the lead's; never edit it)

## Doing

A1: the C1 rerun at 100 and 1,000 tenants of 1,000 vectors, queued as jobs. Harness in strata
`bench/hybrid/` at strata:ba14ec11 (`c1.py` driver, `c1_strata.cc` strata arm, `setup.sh`,
`CMakeLists.txt`); baselines installed under the worktree's `.scratch/c1` (setup log there).

Local smoke at 5 tenants (light hold, busy host, 2 s idle; not evidence) ran every system end to
end: recall@10 1.0 for strata, pgvector, Qdrant and LanceDB, 0.63 for Elasticsearch (HNSW,
num_candidates 50); idle footprint strata 4 MB, pgvector 17 MB, Qdrant 70 MB, LanceDB 73 MB,
Elasticsearch 8,728 MB (8 GiB heap).

Jobs (each row waits on its load, each load on the row before; the disk holds one system's
1,000-tenant data at a time, so `row` removes it):

| What | Load (light) | Row (exclusive, A1) |
|---|---|---|
| dry run, every system at 100 (light, 8b64cca8) | 261118-000128-f4c57b | |
| strata 100 | 261118-000217-1b2d99 | 261118-000222-daaa02 |
| lance 100 | 261118-000228-b02de5 | 261118-000232-455a6e |
| pg 100 | 261118-000237-45bc8b | 261118-000241-b5b8ea |
| qdrant 100 | 261118-000246-72534e | 261118-000250-c3e06a |
| es 100 | 261118-000254-4dc53b | 261118-000258-e12173 |
| strata 1000 | 261118-000304-ed9a17 | 261118-000309-f80aab |
| lance 1000 | 261118-000314-30a389 | 261118-000318-4a0dcd |
| pg 1000 | 261118-000323-3324e0 | 261118-000327-8ad2ec |
| qdrant 1000 | 261118-000332-ad86a6 | 261118-000336-691a42 |
| es 1000 | 261118-000341-8b10b1 | 261118-000345-676fcd |

Results land in `.scratch/c1/results/SYS-N.json` (and `-load.json`); job logs carry the same JSON.

## Next

1. When the rows finish: tabulate them into strata `docs/perf/c1-strata-tenants.md` (results
   note: jobs, commits, host) and the fit verdict's C1 reading; rerun failed links with
   `campaign job rerun`.
2. A2: the optimization experiments (i8 codes per Q5, fewer files per index, the catalog's
   idle tiers), each a before/after at named commits.

## Setup (stated before installing)

Nothing system-wide: every service goes under the strata worktree's ignored
`.scratch/c1/` (`bench/hybrid/setup.sh`), run natively on this Mac (i9-9980HK, 16 threads,
32 GiB, macOS 15), stopped and its data deleted after each row.

| System | How C1 ran it (strata `docs/perf/c1-field-tenants.md`, 2026-10-27) | Here |
|---|---|---|
| Qdrant | Docker `qdrant/qdrant:latest` 1.20.0, Linux VM | Native release binary 1.20.2 (x86_64-apple-darwin) |
| pgvector | Docker `pgvector/pgvector:pg17` (PG 17.11, pgvector 0.8.7) | PG 17.11 and pgvector 0.8.7 built from source into `.scratch/c1/pg` (Homebrew's PG 14 is untouched; it has no pgvector) |
| Elasticsearch | Docker 9.5.5 | Native darwin-x86_64 tarball 9.5.4 (9.5.5 has no x86_64 macOS build) |
| LanceDB | pip 0.40.0, Linux | pip 0.25.3 in a Python 3.12 venv (the last release with x86_64 macOS wheels) |
| strata | not run | `bench/hybrid/c1_strata.cc` at the quest branch (main's code) |

Docker Desktop is installed but not running; it is not used (a Linux VM beside a native strata
would mix two kernels and a port proxy into p99).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- Tenant counts at 1,000 vectors × 768-d float32: 100 (dry run) and 1,000 (3 GB raw). 10,000
  tenants (30 GB raw per system) does not fit the host's free disk (27 GiB at 2026-11-17) and is
  reported as not measured on this host.
- Load is a light job; each row's measurement (service restarted on the loaded data, 30 s idle,
  memory, 1,000 cold then warm queries, memory) is one exclusive job ≤10 min.
- strata stores each vector as a document's stored value (float32, 3,072 bytes) in one index
  per tenant behind a `catalog`, and answers by an exact scan: no vector code exists on main.
- Elasticsearch: 8 GiB heap (`-Xms8g -Xmx8g`), C1's (its default on that 16 GiB VM; the
  default here would be 16 GiB); disk watermarks off, since this host's disk is past the 95%
  flood stage, which would make the index read-only.
- One shape per system, each C1's working one: Qdrant payload tenancy with bulk upload, pgvector
  shared table plus btree(tenant) (exact), Elasticsearch shared index with filtered kNN, LanceDB
  table per tenant (flat), strata index per tenant. Collection, index and table per tenant hit
  hard limits at about 1,000 in C1 and are not rerun.
- Latency: services are timed at the Python client around each call (HTTP or libpq on
  localhost); strata and LanceDB are timed inside their process (embedded engines, a child
  process on a line protocol). Memory and idle CPU come from `proc_pid_rusage` on each
  process tree: footprint (dirty private, the anon analogue), resident, CPU seconds over 30 s.
- strata's catalog runs as Xapiand's host does: `defer_io`, the sweep on, 8 chores.
- Recall@10 against numpy's exact top 10 is recorded per pass.
- No page-cache drop: `purge` needs root, which this host's agents lack; "cold" is a restarted
  service's first touch with the OS cache as the load left it.
