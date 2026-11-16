# phase6: Phase 6, async backends and tuning

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase6`.

## Outcome

Storage reads leave the reactor through io_uring and POSIX AIO residency backends and a verified block
cache, and scheduling is tuned, so a loaded Xapiand keeps its reactors responsive: p99 stall at most
1 ms under the mixed heavy and light load, light queries under heavy load within 2x their unloaded p99
plus 1 ms, at most one major reactor fault per 1,000 queries with the index at twice RAM. strata beats
every baseline by Phase 6's margins on all four axes. This quest is the phase's gate; the lead splits
its items into quests in `residency`, strata and prospect when the phase starts.

## Acceptance criteria

From strata `docs/PLAN.md`, "Phase 6: async backends and tuning" (items and gate) and "Ratchets" (the
Phase 6 rows), with Phase 7c's T6 row, which the owner gated here (2026-10-29, D15).

- A1 io_uring and POSIX AIO residency backends, and the verified block cache: tests in jobs on Linux and FreeBSD
- A2 Gate: reactor p99 stall at most 1 ms under the mixed heavy and light load: `stall_load.py` jobs
- A3 Gate: light-query p99 under heavy load at most 2x its unloaded p99 plus 1 ms, against the baseline Phase 0 measured (7.2 ms offloaded) and 7c's pre-7c baseline: `stall_load.py` jobs
- A4 Gate: major reactor faults per 1,000 queries at most 1 with the index at twice RAM (7c's T6, `getrusage(RUSAGE_THREAD)` per loop on the Linux VM): jobs
- A5 Gate: queries at most 0.7x the reference on every class, PISA binding (QG-R28, F13-R45): gate VM jobs
- A6 Gate: single-thread indexing at least 1.3x: gate VM jobs
- A7 Gate: indexing and serving memory at most 0.7x; zero heap allocations per query in steady state on the term, conjunction, disjunction and phrase paths: jobs
- A8 Gate: disk at most 0.8x the whole index and 0.9x every section: jobs
- A9 Every gate: ASan, UBSan, TSan; fuzzing for each decoder added or changed; coverage; the scale statement: jobs
- A10 [artifact] The gate report, every row with its commit, platform and jobs: `docs/gates/phase6.md`

## Contracts

Consumes the `residency` interface frozen at Phase 1 (its synchronous backend) and 7c's reactor model
(`docs/design/phase7c.md`).

## Decisions

- T6 of 7c is reported at 7c and gated here (owner, 2026-10-29, 7c Q5, D15).
- Bare-metal rows (BENCHMARKS rule 4): the Phase 0 review moved the hop and light-query baselines to
  bare metal; the M1 is retired (PLAN open question 9).
- Follows the release (D39). A2 and A3 gate it already as 7c's T4 and T5 (`x7-gate` A4, A5); A4
  stays here (D15), reported at 7c as T6 and stated in the release notes.

## May land early

Each backend behind its own option, with its tests.
