# strata-engine-search (S2, phase 7a)

> Status: CURRENT (2026-10-27) workstream `strata-engine-search`; branch `phase7a/strata-engine-search` from `phase7a/strata-engine-write` (Xapiand:b078d5a18). Local only, not pushed, no CI run.

Design: `strata/docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S2, G5, G7, D13, T6, T7, T11.

## State

In progress: first build pending.

## Done criteria (design row S2)

`engine::search` on the strata engine for term, boolean, phrase and near nodes; estimates; `refresh` before
search; docid tie-break; the default node's five shards searched as one index.

1. `engine::search` dispatches on the shards' engine; a search over indexes of two engines is refused.
2. Terms, `match_all`, AND, OR, AND_NOT, AND_MAYBE, FILTER, SCALE_WEIGHT, NEAR, PHRASE (over terms) and append chains.
3. One snapshot over every segment of every shard: Xapian's docid interleave `(did - 1) * n + shard + 1`,
   global statistics, the weights of prospect's `bm25_xapian` preset.
4. The estimate is the exact count of live matches (G12); the write buffer is flushed before a search.
5. Ties by docid in the requested order (ascending, descending).
6. Everything else refused with a client error that names it (`errc::syntax`): sorter, collapse, spy, similar
   documents, cutoffs, XOR, MAX, SYNONYM, ELITE_SET, wildcard, edit distance, posting sources, query text.
7. Tests shared with the Xapian engine; the docs suite on a strata-backed index for the search subset;
   ASan; a mutant per rule; a query smoke against the Xapian engine (screen only).
8. `docs/mechanisms/strata-search.md`.

## Done

(nothing committed yet)

## Next

Build, unit tests, ASan, harness runs, mechanism note.

## Blocked on

Nothing. Inputs: S1 (`phase7a/strata-engine-write`, rebased on at b078d5a18), prospect P3 `phase4/xapian-ops`
(pinned at its pushed head prospect:c60547f for `operators.hh`, until it lands on prospect main), prospect P4
`phase4/host-collectors` (not started: the multi-index collection here is Xapiand's own and moves to P4's when it lands).
