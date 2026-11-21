# restore-stream: a whole-database RESTORE streams a top-level MessagePack array element by element

Risk: high (a decoder of untrusted input; the review carries the correctness questions too). The
lead writes this quest; contributors read it and never edit it. Its machine state is in
`state.json`: `campaign status restore-stream`.

## Outcome

A whole-database RESTORE of one MessagePack array (the es-*.msgpack dumps) reaches the indexer as
one object: the unpacker yields it only after the last byte, and the indexer holds every element's
prepared state until the commit, so restoring Kibana's logs sample (14,005 docs, 45.6 MB index)
peaks at 1,329 MB and shakespeare (111,396 docs, 19.4 MB) at 1,824 MB. The explorer's prototype
(explore/bulk-load-rss, Xapiand:b60d09d64, docs/perf/bulk-load-rss.md there) skips the array's
header so the body streams as its elements: 339 and 396 MB, times unchanged. This quest builds
that properly: the array's declared count is enforced, nothing a dump could say before is
accepted or rejected differently, and the remaining peak is stated.

## Acceptance criteria

- A1 Restoring logs and shakespeare as one array peaks at most 450 MB, measured as `strict` jobs
  with the probe (harness/bench/bulk_rss_probe.py from the explore branch), before and after at
  named commits
- A2 Load time does not regress: the same `strict` jobs, five trials each, with an A/A; a
  slowdown past the A/A's spread is a failure, stated with numbers
- A3 Identity: after restoring each dump as one array and as the same objects streamed without the
  array, both databases hold the same documents (ids and full contents): a test or job with values
- A4 The declared count holds: an array whose header promises more elements than the body holds,
  and an array followed by more data, each answer an error naming it, never a 200; elements applied
  before the error stay as the NDJSON stream leaves them after a bad line (or the journal states
  the difference): a test per case, each failing at the prototype b60d09d64 (`--fails-without`)
- A5 Nothing else changes: a nested array element, a non-map element, an empty array, a header
  split across chunks (every split point of the 1, 3 and 5 byte headers), a single document
  RESTORE and the NDJSON stream answer as they do on master, except where master answers 200
  while dropping input (a too-deep or malformed element): that answers an error (D162). Tests with
  the responses' values, master's captured first
- A6 The decoder is fuzzed: the stream feed (header skip and count) under ASan+UBSan for at
  least 30 CPU-minutes, no crash, leak or timeout, recorded with the executed-unit count
- A7 [artifact] `docs/perf/bulk-load-rss.md`: the breakdown, before and after with job ids, and
  what holds the remaining peak (strata's write buffer until the one commit)
- A8 [ci] CI is green on every platform

## Contracts

None: RESTORE's accepted inputs and responses do not change (A5).

## Decisions

- Lead, 2026-11-21: the explorer's note lands with this quest, not separately (an explore merge
  lands only its note, and landing it on master alone would restart every landing queue's CI).
  The prototype expands a nested array element instead of rejecting it; that change is not taken.
- Lead, 2026-11-21 (D162): where master answers 200 while indexing nothing or dropping part of the
  body (a short array, trailing data after the array, a malformed or too-deep element), the
  stream answers an error instead: errors never pass silently. CHANGELOG names it.

## May land early

Nothing: one landing.
