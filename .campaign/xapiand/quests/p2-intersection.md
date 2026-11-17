# p2-intersection: Branch-free block compare and per-word rank in intersections

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status p2-intersection`.

## Outcome

track-p's P2 experiment (prospect `docs/perf/p2-intersection.md` on `quest/xapiand/track-p`,
prospect:6960df8; prototype `p2-intersection.patch`) kept two changes: bit 1 compares all 128
slots of a decoded block without branches, bit 2 keeps a rank per bitset word. At 1M documents
(job 261117-021758-5aded6, exclusive, 7 rounds, A/A 0.97-1.04) intersections run at 0.78-0.92 of
main's time, 14 of 15 rows win, none lose; results byte-identical. This quest lands them in
`include/prospect/reader.hh` without the experiment switch.

## Acceptance criteria

- A1 Both changes always on, no switch; unit tests with values for the branch-free compare
  (full, partial and empty blocks, the last block) and the per-word rank (word boundaries, an
  all-ones and an all-zeros word), each with a `--fails-without` mutant
- A2 Identity: the dump of the 1M index's query results equals main's byte for byte, and verify
  finds 0 mismatches (a job)
- A3 The intersection classes' A/B against main at the landing commit, exclusive, with an A/A,
  at least 5 rounds: no row slower than the A/A spread, and the gain stated (a job)
- A4 GitHub CI green on every platform (arm64 and Linux included)

## Decisions

- Lead: galloping across blocks and block-against-block filtering for counted conjunctions were
  measured and removed (the P2 entry); not part of this quest.

## May land early

Nothing.
