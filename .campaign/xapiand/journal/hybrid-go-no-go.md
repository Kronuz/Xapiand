# hybrid-go-no-go: journal

State: waiting on jobs 261118-002817-ca72e9 and the VM chain in Doing (last: 261118-003018-148235)
Quest: .campaign/xapiand/quests/hybrid-go-no-go.md (the lead's; never edit it)

## Doing

A1: C1 rerun at 100 and 1,000 tenants of 1,000 vectors, deciding rows in document-gate's Linux
VM (lead, 2026-11-18). Harness strata:ed42f792, `bench/hybrid/`: `c1.py`, `c1_strata.cc`,
`vm.sh` (VM wrapper: boots 8 vCPU / 16 GiB on SSH 18782, syncs the checkout, builds
c1_strata, runs, copies results to the worktree's `.scratch/c1-vm/`, stops the VM on every
exit), `setup.sh` (macOS dry-run installs).

Lead's corrections, folded in at strata:ed42f792:
1. Recall: each ANN system's search parameter is tuned at load to recall@10 >= 0.95 on 200
   queries (Elasticsearch num_candidates 50 to 1,000, then rescore oversample 10; Qdrant
   hnsw_ef 128 to 1,024), saved as `SYS-N-tune.json`; every pass records recall@10 and a row
   below 0.95 fails. pgvector (shared table), LanceDB (flat) and strata are exact.
2. Environment: services in Docker at C1's images (Qdrant v1.20.0, pgvector 0.8.7-pg17,
   Elasticsearch 9.5.5), LanceDB 0.40.0, strata built in the VM. macOS native is a dry run only,
   labelled `env: macos-dry-run` in its results.
3. Cold: in the VM the page cache is dropped (`echo 3 > drop_caches` as root) after the 30 s
   idle and before the first pass, which is then `cold`; without a drop (macOS) it is
   `first_warm`.

Superseded: the macOS chain at strata:a950f1a4 (cancelled); its dry run 261118-002035-ee3342
was replaced by the one below. First dry run 261118-000128-f4c57b (strata:8b64cca8): all but
Qdrant passed (one connection reset; resend fix strata:a950f1a4).

Jobs (each row waits on its load, each load on the row before):

| What | Load + tune (light) | Row (exclusive, A1) |
|---|---|---|
| macOS dry run, every system at 100 (not evidence) | 261118-002809-0a06e7 | |
| VM setup | 261118-002817-ca72e9 | |
| strata 100 | 261118-002825-8d62f7 | 261118-002831-fab1cd |
| lance 100 | 261118-002841-6e58d7 | 261118-002847-822bcf |
| pg 100 | 261118-002854-03f894 | 261118-002859-39b455 |
| qdrant 100 | 261118-002908-5c7ce8 | 261118-002914-68b7f1 |
| es 100 | 261118-002922-74866d | 261118-002927-fc4e42 |
| strata 1000 | 261118-002934-0bbebe | 261118-002938-82a6af |
| lance 1000 | 261118-002946-0cc4c0 | 261118-002951-87a425 |
| pg 1000 | 261118-002957-1432d6 | 261118-003001-d27a1c |
| qdrant 1000 | 261118-003005-18f90e | 261118-003009-c2aab9 |
| es 1000 | 261118-003014-529248 | 261118-003018-148235 |

## Next

1. When the rows finish: tabulate into strata `docs/perf/c1-strata-tenants.md` (jobs, commits,
   VM, recall and tuned parameter beside every row; "warm" wherever no cache drop; 10,000
   tenants out until disk allows) and the fit verdict's C1 reading. Rerun failed links with
   `campaign job rerun`.
2. A2: optimization experiments, each a before/after at named commits.

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

Deciding rows (lead, 2026-11-18): document-gate's Linux VM, where `vm.sh setup` installs
docker.io, python3-venv, cmake and ninja inside the VM (not on the host), pulls C1's images and
builds a LanceDB 0.40.0 venv. The macOS installs above serve the dry run only.

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

- Tenant counts at 1,000 vectors × 768-d float32: 100 and 1,000 (3 GB raw). 10,000 tenants
  (30 GB raw per system) is out until disk allows (27 GiB free on the host, which holds the VM's
  image); the results doc states it.
- Load is a light job; each row's measurement (service restarted on the loaded data, 30 s idle,
  memory, 1,000 cold then warm queries, memory) is one exclusive job ≤10 min.
- strata stores each vector as a document's stored value (float32, 3,072 bytes) in one index
  per tenant behind a `catalog`, and answers by an exact scan: no vector code exists on main.
- Elasticsearch: 8 GiB heap (`-Xms8g -Xmx8g`), C1's (its default on a 16 GiB machine). On the
  macOS dry run only, disk watermarks are off (that disk is past the 95% flood stage).
- Each VM job boots and stops the VM itself (trap on exit), so no batch leaves it running.
- One shape per system, each C1's working one: Qdrant payload tenancy with bulk upload, pgvector
  shared table plus btree(tenant) (exact), Elasticsearch shared index with filtered kNN, LanceDB
  table per tenant (flat), strata index per tenant. Collection, index and table per tenant hit
  hard limits at about 1,000 in C1 and are not rerun.
- Latency: services are timed at the Python client around each call (HTTP or libpq on
  localhost); strata and LanceDB are timed inside their process (embedded engines, a child
  process on a line protocol). Memory and idle CPU: in the VM, a container's cgroup
  (anon, rss = anon + mapped file, C1's measure) and `/proc` for the embedded engines (RssAnon,
  VmRSS); on macOS `proc_pid_rusage` (phys_footprint as anon). CPU seconds over 30 s idle.
- strata's catalog runs as Xapiand's host does: `defer_io`, the sweep on, 8 chores.
- Recall@10 against numpy's exact top 10 is recorded per pass.
- No page-cache drop on macOS (`purge` needs root): its first pass is `first_warm`, never cold.
