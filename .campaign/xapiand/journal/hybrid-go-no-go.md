# hybrid-go-no-go: journal

State: working
Quest: .campaign/xapiand/quests/hybrid-go-no-go.md (the lead's; never edit it)

## Doing

A1, step 1: the C1 rerun harness in strata `bench/hybrid/` (quest branch, main merged at
strata:762b748b) and a light dry run at the smallest tenant count.

## Next

1. Commit `bench/hybrid/` (c1.py driver, c1_strata.cc arm, setup.sh); dry run (light) at 100 tenants.
2. Rows, one exclusive job each (≤10 min), load phases as light jobs before them.

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
- No page-cache drop: `purge` needs root, which this host's agents lack; "cold" is a restarted
  service's first touch with the OS cache as the load left it.
