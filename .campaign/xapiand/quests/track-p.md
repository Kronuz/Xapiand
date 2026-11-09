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
- A2 P3: variable-size blocks for block-max pruning, and impact-ordered evaluation for top-k on long lists: a `docs/perf/` entry from jobs
- A3 P4: partitioned Elias-Fano and Roaring containers as alternative block encodings, chosen per block: a `docs/perf/` entry from jobs
- A4 P5: next-word (common-grams) positional entries for frequent word pairs, with the index-size cost: a `docs/perf/` entry from jobs
- A5 P7: membership probes on dense terms (prospect `docs/perf/p7-membership-probes.md` exists: its verdict and review recorded): jobs
- A6 P8: phrase prefilters, per-block summaries of frequent adjacent pairs (format 13 reserves its section kind): a `docs/perf/` entry from jobs
- A7 P9: a cost-based planner choosing leapfrog, windows, probes or bitmap AND per clause: a `docs/perf/` entry from jobs
- A8 P10: distributed threshold sharing across shards (after Track D's D5): a `docs/perf/` entry from jobs
- A9 P11: count and aggregation answers from per-segment statistics and sketches where exact, extended to cross-shard plans: a `docs/perf/` entry from jobs
- A10 P13: positions with a per-block choice among FOR, PFOR and Elias-Fano, and deltas coded against the document's length: a `docs/perf/` entry from jobs
- A11 P14: stored fields with dictionary-trained zstd per segment and block sizes tuned for fetch latency (format 13 reserves its section kind): a `docs/perf/` entry from jobs
- A12 P20: frozen indexes, single-node half after Phase 3's gate (strata `docs/design/frozen-index.md`), measured against the same index unfrozen and against PISA: a `docs/perf/` entry from jobs
- A13 P21: threshold seeding, each term's k-th best score for k = 10, 100 and 1,000 stored at merge, measured against PISA (format 13's seeds, prospect `docs/perf/format13-seeds-cost.md`: its verdict recorded): jobs
- A14 The Phase 3 committed items' gap rows: intersection and negated classes (P2, P7, P9), phrases (P8, P5), `intersection_union` (block-max WAND for nested disjunctions, review C7), frequent terms (the frontier cost, review C15, and P3), each measured against the gap it closes: jobs

## Contracts

Format-dependent items (P4, P8, P13, P14, P20, P21) change prospect's segment format: pre-freeze bumps
under strata's "Formats stay open until the first release" (D30), each with fixtures, digest identity,
decoder fuzzing and a revert check; only the decoder's safety gets a review.

## Decisions

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
