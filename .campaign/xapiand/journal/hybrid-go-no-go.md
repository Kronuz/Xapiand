# hybrid-go-no-go: journal

State: A1 to A3 done (strata:08f372bc); A4 waits on the owner (decision request in Result). No jobs running.
Quest: .campaign/xapiand/quests/hybrid-go-no-go.md (the lead's; never edit it)

## Doing

Nothing: A1 to A3 written up at strata:08f372bc; A4 is the owner's.

## Next

1. Owner answers A4 (Result); record it as a Decisions row.
2. Main changed 6 files since the branch: merge main before landing (a code merge, so evidence
   reruns at the merge; the lead's call when).
3. Push the branch for CI (contributors do not push).

## Resumed 2026-11-20

The A2 chain and the Qdrant and ES 1,000 rerun (the lead's, one chain at strata:7b02184a) all
passed: rows 261119-173351-6dabcb, -173357-5a1aac, -173402-06e84b, -173414-b5197c,
-173419-733f81, -173425-b0afb9 and their loads. Raw JSON copied into
`docs/design/hybrid-search/c1-rerun/` (Qdrant, ES at 1,000) and `c1-rerun/a2/` (strata f32 and
i8 at 7b02184a; A1's strata rows at ed42f792 and 3e8dcb93 stay as they were). A1 jobs
(261118-*) re-checked: all done, exit 0, exclusive.

Reading at 1,000 tenants (100 in c1-rerun.md):
1. Warm p99: strata f32 1.32 ms (A1), i8 0.69; Qdrant 4.10 / 8.20 (warm1 / warm2), pgvector 7.38,
   ES 11.9, LanceDB 14.0. f32 3.1x faster than Qdrant's better pass; i8 6.0x.
2. Idle anon: strata 3.4 MB; pgvector 4.3; LanceDB 72; Qdrant 104 + 3,026 mapped (rss 3,129);
   ES 8,787 heap, 15.8 CPU s per 30 s idle after queries.
3. Cold p99: strata f32 12.9 (A1) / 10.8 (A2 chain); i8 5.74; Qdrant 4.69 (mapped, not cold);
   pgvector 20.1; LanceDB 21.4; ES 66.5. i8: 1.9x faster than f32 at the same commit; 1.2x
   slower than Qdrant (1.6x net of its client ms).
4. $4 per GiB-month (idle-index.md's assumption): strata $0.01 a month per 1M vectors resident;
   Qdrant $12.20 with its mapped collection; ES $34.30 plus $15.40 idle CPU.

## Resumed 2026-11-19

build-test 261118-112804-365541 at strata:ff10ac80 failed on one test, manifest_log (`replay cost`,
`../test/manifest_log_test.cc:1050: CHECK(fields_ratio < 40)`, measured 63.6x): a timing ratio
under a light job's load, in code this quest never touches (its file set is docs and bench/hybrid);
74 of 75 passed. Main merged (strata:80be34f0, no rebase). Resubmitted via `campaign verify` at the
new head. A2 (option a of Blocked on 2): `bench/hybrid` gained a `strata-i8` arm (strata:7b02184a,
vectors stored as a float32 scale and 768 int8 codes, 772 bytes against 3,072, scored against the
float32 query); a before and after at that one commit, f32 and i8 at 100 and 1,000 tenants, one VM
chain. Recall@10 is recorded per pass (bar 0.95; the exact column is unchanged f32 in numpy).

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

1. A4: the owner's go or no-go (the decision request in Result). Not decided here.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | Rows, exclusive, VM: 261118-002831-fab1cd (strata 100), 261118-002859-39b455 (pg 100), 261118-002847-822bcf (lance 100), 261118-044446-7f14e4 (qdrant 100), 261118-044448-81dd25 (es 100), 261118-044452-9ca15f (strata 1,000), 261118-044458-428db2 (pg 1,000), 261118-044455-07f613 (lance 1,000), 261119-173419-733f81 (qdrant 1,000), 261119-173425-b0afb9 (es 1,000); loads 261119-173416-dd03e0, 261119-173422-9a6925 |
| A2 | `i8` against `f32`, both at strata:7b02184a (the i8 arm's commit; f32 is main's storage), one chain: rows 261119-173357-5a1aac (f32 100), 261119-173351-6dabcb (i8 100), 261119-173414-b5197c (f32 1,000), 261119-173402-06e84b (i8 1,000); loads 261119-173354-6399a6, 261119-173348-7e0d04, 261119-173408-a8663d, 261119-173359-d32394 |
| A3 | `docs/design/hybrid-search.md` (C1 rerun and fit verdict, strata:08f372bc; tables in `docs/design/hybrid-search/c1-rerun.md`) |
| A4 | D155 |

## Result

A1 to A3 done at strata:08f372bc. A4 is the owner's: decision request below.

**Decision request (A4): go on hybrid steps 1 to 4, or drop "best at AI retrieval".**

Facts (1,000 tenants of 1,000 × 768-d vectors, Linux VM, exclusive jobs; 100 tenants agree):
1. Warm p99: strata 1.32 ms with no vector code, 0.69 ms with `i8` codes; Qdrant 4.10 ms
   (its better pass), pgvector 7.38, Elasticsearch 11.9, LanceDB 14.0. strata 3.1× faster than
   the best other, 6.0× with `i8`.
2. Idle cost: strata 3.4 MB anon, $0.01 a month per 1M vectors at $4 per GiB-month; pgvector
   4.3 MB (parity); LanceDB 72 MB; Qdrant 104 MB plus 3,026 MB mapped ($12.20); Elasticsearch
   8.8 GB heap ($34.30) plus half a core idle ($15.40).
3. Cold p99: Qdrant 4.69 ms, but only because it keeps the whole collection mapped (not evicted
   by `drop_caches`). strata 12.9 ms (`f32`), 5.74 ms with `i8` (1.9× faster at one commit; 1.2×
   behind Qdrant, 1.6× net of Qdrant's client overhead); 3.5× faster than pgvector, 3.7× than
   LanceDB, 11.6× than Elasticsearch.
4. The bound: `i8` cut bytes 3.9× but cold time 1.9×; about 4 ms per cold tenant open is now
   70% of the p99. `bit1` estimates to about 4.3 ms (Qdrant's level, two-point fit, not run); a
   clear cold lead also needs the per-tenant open cut, which is engine work, not vector work.
5. Not measured: 10^4 and 10^5 tenants (disk), Qdrant at equal memory (truly cold).

Options:
1. Go, steps 1 to 4, with cold p99 as step 4's condition (Q1 (a)). Steps 1 to 3 (`vecdist`, the
   exact path, fusion and the DSL) build on a measured warm and idle lead; T8 at 10^5 tenants
   decides the claim.
2. Go without conditions (Q1 (b)): claim "best" now. Not supported: cold p99 trails Qdrant.
3. Drop "best at AI retrieval" (Q1 (c)): vectors as table stakes (exact plus fusion), no ANN
   program, no "best" claims.

Recommendation: option 1. The leads hold with no vector code: warm p99 3.1× to 20× faster,
idle memory at the field's floor (pgvector parity, 21× to 2,560× below the rest); the one gap, cold p99 against a system paying 3 GB resident for it, closed from
2.7× to 1.2× with the first optimization, and the rest of it is the per-tenant open the cold tier
already works on. Q2 to Q6 stand as ruled 2026-10-27; Q1 is this decision.

What deferring costs: steps 1 to 3 are worth building under option 3 too (the design's Q1 note),
so deferring delays `vecdist` and the exact path, not only the claim; the evidence decays as
main moves (strata's own p99 drifted 15% to 30% between A1 and A2 commits), so a later
decision needs the rows rerun (Qdrant and Elasticsearch loads alone take 16 and 19 minutes at
1,000 tenants); the Q1 "not yet" has stood since 2026-10-27.

## Rulings I made

- Results live in `docs/design/hybrid-search/c1-rerun.md` (the quest's file set), not
  `docs/perf/`; raw JSON beside it in `c1-rerun/`.
- A2 rows live in `c1-rerun/a2/`; A1's strata rows keep their older commits. Against the field,
  warm p99 is quoted at each system's better warm pass where the two differ (Qdrant 1,000:
  4.10 against 8.20 ms), the conservative reading for strata.
- Dollars at `docs/perf/idle-index.md`'s assumed $4 per GiB-month, plus $0.04 per core-hour
  (`format-14-kv/q1-q2.md`); both stated as assumptions in the doc.
- No new job: every planned row is present and valid; Qdrant truly cold is a new experiment
  (memory-limited), named as not measured rather than run.
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
