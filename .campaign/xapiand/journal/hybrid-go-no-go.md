# hybrid-go-no-go: journal

State: waiting on jobs 261118-112804-365541 (build-test at strata:ff10ac80); A1 rows read, verdict written (A3); A2 and A4 open (Blocked on). Strata:ff10ac80.
Quest: .campaign/xapiand/quests/hybrid-go-no-go.md (the lead's; never edit it)

## Doing

Results read and written up (strata:ff10ac80): `docs/design/hybrid-search/c1-rerun.md` (table,
jobs, raw JSON in `c1-rerun/`) and the fit verdict's "C1 rerun (2026-11-18)" in
`docs/design/hybrid-search.md`. Main merged (strata:40788b0e, no rebase).

Reading (100 / 1,000 tenants of 1,000 vectors, VM):
1. Warm p99: strata 1.26 / 1.32 ms; 3.1x faster than Qdrant (100), 4.4x / 5.6x than pgvector,
   7.6x than ES (100), 10.2x / 10.6x than LanceDB.
2. Idle anon: strata 3.4 MB at both; pgvector 4.3; Qdrant 46 (+382 MB mapped); LanceDB 72; ES 8,802.
3. Cold p99: strata 11.9 / 12.9 ms; Qdrant 4.0 (100; its mapped pages survive drop_caches), so
   3.0x slower than Qdrant, 1.3x / 1.6x faster than pgvector, 1.3x / 1.7x than LanceDB, 4.4x than ES.
4. Recommendation: go on steps 1 to 4, cold p99 the condition (A2: i8 codes read cold).

Failures attributed (also in c1-rerun.md):
1. 261118-000128-f4c57b: first dry run ran on macOS; Qdrant reset (fixed a950f1a4), ES recall
   0.5655 (fixed 3e8dcb93). Not evidence.
2. 261118-002809-0a06e7: macOS dry run, Qdrant 409 from a stale Qdrant on the port (fixed 72990a82).
3. 261118-044500-19572d: VM powered off from outside 5.5 min into Qdrant's 1,000 load: console
   (`~/.scratch/vm-document-gate/linux/console.log`) shows a clean systemd shutdown begun while
   the SSH session was open, no OOM or panic; no campaign job drove the VM in 10:10-10:16. Its
   chain (qdrant row c7b37f, es load 2593f7, es row 13047d) was skipped.
Timeout 261118-002908-5c7ce8 (Qdrant 100 load, 12 min, no log): rerun 9eed59 loaded in 95.5 s.

## Next

1. Lead: rerun 19572d, c7b37f, 2593f7, 13047d (`campaign job rerun --commit 3e8dcb934251 ...`);
   my rerun was refused by the permission classifier. Then fold their rows into c1-rerun.md.
2. A2: before/after of an `i8` code column read cold against today's `f32` stored values.
3. Push the branch for CI (contributors do not push).

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

1. A4: the owner's go or no-go and Q1 to Q6 (recommendation: go on steps 1 to 4, cold p99 the
   condition; Q2 to Q6 stand as ruled 2026-10-27, Q1 is this decision).
2. A2: not run. Options: (a) an `i8` code-column prototype in bench/hybrid, cold and warm
   before/after at named commits (recommended, a few hours); (b) defer A2 to step 4's gate rows.
3. Qdrant and ES at 1,000 tenants: rerun needs the lead (Next 1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-002831-fab1cd, 261118-002847-822bcf, 261118-002859-39b455, 261118-044446-7f14e4, 261118-044448-81dd25, 261118-044452-9ca15f, 261118-044455-07f613, 261118-044458-428db2 (Qdrant and ES at 1,000 missing) |
| A2 | <no evidence yet> |
| A3 | strata:ff10ac80 `docs/design/hybrid-search.md` (C1 rerun) and `docs/design/hybrid-search/c1-rerun.md` |
| A4 | <no evidence yet> |

## Result

Partial: A1 rows at 100 (all five) and 1,000 (strata, pgvector, LanceDB) and A3 in strata:ff10ac80
(`docs/design/hybrid-search/c1-rerun.md`). A2 and A4 open.

## Rulings I made

- Results live in `docs/design/hybrid-search/c1-rerun.md` (the quest's file set), not
  `docs/perf/`; raw JSON beside it in `c1-rerun/`.
- Warm is the second warm pass (C1's convention), the first pass's p99 shown beside it; ES idle
  CPU right after start (28.4 s) is startup work, so its after-queries 0.52 s is quoted too.

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
