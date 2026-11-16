# distributed-search: Fan-out, exact statistics, merged aggregations, patterns from the root

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status distributed-search`.

## Outcome

A search from any node spans the cluster (design D10). The coordinator resolves names and patterns
from the root's namespace records, runs `search_batched` with local shards in process and remote
ones through `search_partial`, and batches one call per node, statistics kept per index. A `stats`
round trip first gathers each index's document count, total length and term frequencies, passed to
prospect's supplied-statistics input. Each shard's statistics and partial come from one replica per
request. Aggregations merge through `serialise_results` and `merge_results`. A shard that misses
the deadline is retried once on another replica, then reported in `_shards.failed`. The local
directory walk is deleted: patterns run the `Glob` automaton over the root's sorted keys, seeking
past subtrees that cannot match, under `walk_gate`'s work bound (F4).

## Acceptance criteria

- A1 Section 1's items 3 and 4, and search, count and every aggregation kind equal over one node
  and over three, five and two-shards-per-node placements (agg-shard-parity's test, cross-node);
  a corpus with replaces included: counts and aggregations exact, ranking compared within one
  replica choice (a job)
- A2 A dropped partial fails A1 (`--fails-without`)
- A3 A replica killed mid-search: the shard is retried on another replica, or the result is marked
  partial within the deadline, with `_shards.failed` naming it (a job)
- A4 Patterns from the root: a search from any node of 3 and 5 returns the same indexes and hits as
  master's one-node walk over the same tree, for `*`, `?`, classes, `**`, `_all`, a leading `*`,
  `**/some-??`, `logs/**`, `-` exclusions and `.` names (never listed), nested paths included (a
  job); the `no-walk` check (`git grep -n "expand(Endpoint::cwd" -- src` empty)
- A5 Scan bound: a pattern scan over 10^6 records holds `walk_gate`'s slot and bound, and a scan
  past the bound is turned away with the gate's status, not left running; `_all` on 10^6 records
  states its time (a test, and a job)
- A6 `index_pattern`'s tests and fuzzers (`tests/test_index_pattern.cc`,
  `tests/fuzz_index_pattern.cc`, `tests/fuzz_index_expand.cc`) run against the root scan, with the
  fuzzers' one CPU-hour each, zero findings (jobs)
- A7 The one extra round trip of exact statistics: search p50 and p99 on 3 processes against one
  node holding the same shards, stated (an `exclusive` job)
- A9 Expansion at the coordinator (prospect-supplied-stats review F1): wildcard, partial and fuzzy queries expand once during the statistics round trip and every shard searches the coordinator's term list, so SYNONYM's df, `most_frequent`'s top N and `Expansion::error` match the merged index; a test over 3 shards compares hit ids, order and score bits with one merged index for each expansion kind, and a mutant that expands per shard fails (`--fails-without`)
- A10 A term missing from the supplied statistics is a host bug, never a client error (review F2): prospect raises its own exception type, Xapiand maps it to an internal error and retries the shard once with fresh statistics; a test forces the race and checks no 400 reaches the client
- A8 [artifact] `docs/mechanisms/distributed-search.md`

## Contracts

Provides `xapiand:search-fanout`: the coordinator, the per-node batch key, the `stats` and
`search_partial` bodies' use, and the root pattern scan. Consumes `prospect:supplied-stats`,
`xapiand:shard-rpc`, `strata:placement-records` and `xapiand:shard-map`.

## Decisions

- Design D5, D10, F4, F7, F11, F20.
- Design D5: a scan away from the root leader reads at `prefix` and may miss an index created
  within that member's last refresh, unless the request asks `read=linearizable` (stated in the
  docs by cluster-api).

## May land early

None.
