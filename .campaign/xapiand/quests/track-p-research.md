# track-p-research: Track P research items not yet started (split per item at start)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-p-research`.

## Outcome

The Track P items moved out of track-p on 2026-11-19, unchanged. Each becomes its own quest (implementation or performance assignment, its file set narrowed) when it starts; this file holds the list and the criteria text.

## Acceptance criteria

- A2 P3: variable-size blocks for block-max pruning, and impact-ordered evaluation for top-k on long lists: a `docs/perf/` entry from jobs
- A3 P4: partitioned Elias-Fano and Roaring containers as alternative block encodings, chosen per block: a `docs/perf/` entry from jobs
- A4 P5: next-word (common-grams) positional entries for frequent word pairs, with the index-size cost: a `docs/perf/` entry from jobs
- A6 P8: phrase prefilters, per-block summaries of frequent adjacent pairs (format 13 reserves its section kind): a `docs/perf/` entry from jobs
- A8 P10: distributed threshold sharing across shards (after Track D's D5): a `docs/perf/` entry from jobs
- A9 P11: count and aggregation answers from per-segment statistics and sketches where exact, extended to cross-shard plans: a `docs/perf/` entry from jobs
- A11 P14: stored fields with dictionary-trained zstd per segment and block sizes tuned for fetch latency (format 13 reserves its section kind): a `docs/perf/` entry from jobs
- A12 P20: frozen indexes, single-node half after Phase 3's gate (strata `docs/design/frozen-index.md`), measured against the same index unfrozen and against PISA: a `docs/perf/` entry from jobs
- A13 P21: threshold seeding, each term's k-th best score for k = 10, 100 and 1,000 stored at merge, measured against PISA (format 13's seeds, prospect `docs/perf/format13-seeds-cost.md`: its verdict recorded): jobs

## Contracts

Per item.

## Decisions

- Split from track-p (lead, 2026-11-19).

## May land early

Each item lands as its own quest.
