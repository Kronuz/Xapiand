# track-p: Track P, the performance research items without a verdict

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-p`.

## Outcome

Every Track P item of strata's plan ends in a `docs/perf/` entry with its four-axis table (query speed,
indexing speed, memory, disk; before and after, interleaved, at least five trials, against the fastest
baseline, on Linux, FreeBSD and macOS, x86_64 and arm64), kept if it wins and removed if it does not,
with `verify` and oracle parity green and the architect's review. P1, P6, P12 and P15 landed with
Phase 3 (`phase3-lifecycle`); the reclamation items have `docs/perf/reclamation.md`; this quest is the
rest. It is a standing track: the lead splits it into one quest per item when one starts, in the
repository the item changes (prospect for search, strata for the core).

## Acceptance criteria

From strata `docs/PLAN.md`, "Track P: performance research (parallel, every phase)": each item's gate
is its before/after table in `docs/perf/` and the architect's review. No entry with a verdict was found
by name on strata or prospect main for these items (2026-11-08).

- A1 P2: SIMD intersection of decoded blocks and galloping across blocks; SIMD rank and select in bitset blocks: a `docs/perf/` entry, kept or removed, from jobs
- A5 P7: membership probes on dense terms (prospect `docs/perf/p7-membership-probes.md` exists: its verdict and review recorded): jobs
- A7 P9: a cost-based planner choosing leapfrog, windows, probes or bitmap AND per clause: a `docs/perf/` entry from jobs
- A10 P13: positions with a per-block choice among FOR, PFOR and Elias-Fano, and deltas coded against the document's length: a `docs/perf/` entry from jobs
- A14 The Phase 3 committed items' gap rows: intersection and negated classes (P2, P7, P9), phrases (P8, P5), `intersection_union` (block-max WAND for nested disjunctions, review C7), frequent terms (the frontier cost, review C15, and P3), each measured against the gap it closes: jobs

## Contracts

Format-dependent items (P4, P8, P13, P14, P20, P21) change prospect's segment format: pre-freeze bumps
under strata's "Formats stay open until the first release" (D30), each with fixtures, digest identity,
decoder fuzzing and a revert check; only the decoder's safety gets a review.

## Decisions

- 2026-11-19 (lead): split. A2-A4, A6, A8, A9 and A11-A13 were never started; they move, unchanged, to quest track-p-research (one quest per item when each starts), so this quest lands A1, A5, A7, A10 and A14 and frees its files for automata and phase5.

- Each item lands only with a before/after measurement and stays only if it wins (Goal 2); one that
  does not win is recorded with its numbers and removed.
- Prior evidence, not to redo: strata `docs/design/query-gap.md` revision 2 and its experiments on
  `design/query-gap`, `perf/query-counters`, `perf/query-exp` (attribution predates the prospect query
  wins: take targets from a fresh measurement); prospect `docs/perf/query-wins.md`,
  `pruning-floors.md`, `count-union-bits.md`; strata `docs/perf/hunt-list.md`.
- The indexing design target (owner, 2026-10-17: 4.5x the fastest baseline, per index on one thread)
  is judged in `research-indexing` and the branches `perf/inverter`, `perf/small-indexes`,
  `perf/index-size`, `perf/indexing-gap` (code not on main; their ledgers are history).

## May land early

Each item alone, with its `docs/perf/` entry.
