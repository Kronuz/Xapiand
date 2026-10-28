> Status: CURRENT (2026-10-27) the strata engine's callbacks, operators and expansions at Xapiand:e8d634c8d (branch `phase7a/strata-engine-callbacks`, local, not landed). Term lists are not wired yet (see "Not built").

# The strata engine's callbacks, operators and expansions

## What

`engine::strata::search` (`src/engine/strata/search.cc`) runs everything a Xapiand search hands the engine
besides terms, booleans and phrases: the operators SYNONYM, XOR, MAX and ELITE_SET, wildcard and
edit-distance nodes, host posting sources, a key maker that orders the hits, and a match spy that sees every
match. The three callback bases (`engine/callbacks.h`) are the same objects the Xapian engine wraps; here
they are called in process, with a `DocView` of the matching document.

## How

```
Query node            strata                                       prospect
----------            ------                                       --------
SYNONYM(terms)        Group (terms, union weight)  per search      synonym_scorer
ELITE_SET(n; terms)   Group: heaviest n by idf     per search      disjunction_scorer
XOR / MAX             children (OR under MAX flattened)            xor_scorer / max_scorer
wildcard, edit dist.  Group: expand() over every segment's         term_scorer per term,
                      dictionary, limit applied, per search        joined by SYNONYM / OR / MAX
source                values::Slot over the slot's column          source_scorer<Slot, PostingSource>

SearchRequest         sorter  -> key per match (memcmp), then weight, then docid in the requested order
                      spy     -> see(SegmentDocView, weight) for every match
                      either one: every match is visited (no pruning), and counts the estimate
```

1. **Groups are worked out once over the whole index.** The terms an expansion stands for, an elite set's
   members and a synonym's weight depend on frequencies, so they use the snapshot's (`Plan::groups`, keyed by
   the immutable node) and every segment then builds the same group. Weights equal those of one shard that
   holds every document (`tests/test_engine_callbacks.cc`, `same_documents`).
2. **SYNONYM** weighs one term at the union's estimated size, `round(N (1 - prod (1 - df_i / N)))`
   (`prospect::synonym_df`); its terms must be plain terms.
3. **ELITE_SET** keeps the `n` terms (10 when unset) with the greatest idf over the whole index, earlier terms
   winning ties, and skips terms the index lacks. Its children must be plain terms.
4. **MAX** reads an OR inside it as more subqueries of the MAX, as Xapian does (`MAX(OR(a, b), c)` is
   `MAX(a, b, c)`); an AND inside it still sums.
5. **Expansions** (`expand.cc`, `pattern.cc`): a wildcard without pattern flags is a prefix; with
   `PATTERN_SINGLE` / `PATTERN_MULTI` it matches the whole term, `?` one character and `*` any run, characters
   being UTF-8 code points. An edit-distance node keeps the terms within the distance of the pattern's tail
   (optimal string alignment: insertions, deletions, substitutions, neighbour swaps; code points) after the
   first `prefix_length` characters, which must match. The terms of every segment's dictionary are merged
   into one sorted set; more than `max_expansion` (0 is no limit) fails the search with `errc::invalid`,
   keeps the first terms, or keeps the most frequent (document frequency over the index; ties keep the earlier
   term). The combiner is SYNONYM, OR or MAX.
6. **Sources.** A `PostingSource` filters one value slot: the scorer steps over the slot's present documents
   in order, asks `accept` and scores `weight`. `max_weight()` of 0 bounds nothing. A slot no document of a
   segment holds is no scorer.
7. **Values.** A document's values are stored in the values plugin, one bytes column per slot; an empty value
   is no value (Xapian's `add_value`), and slot 0xFFFFFFFF is refused. `SegmentDocView` reads data, version
   and values lazily from a pinned segment; `Shard::get` and `scan` copy them (`OwnedDocView`).
8. **Key makers.** Each match's key is computed once, from the same `DocView`; hits order by key (bytewise,
   ascending), then weight (heavier first), then document id in the requested order. The estimate is the exact
   match count.
9. **Spies** see every match, once, with its weight, in segment order; `check_at_least` and the window do not
   limit them. The `DocView` passed to a spy carries the document's id across the shards (the Xapian engine's
   carries its id within the shard). A spy or key maker that throws fails that search only.

## Why this mechanism

- Callbacks stay callbacks (D4): the same three bases serve both engines, so a native replacement removes a
  callback without touching its callers.
- Computing groups from the whole snapshot is what makes five shards weigh like one index. On five shards
  Xapian's weights for the terms of a wildcard or edit-distance expansion are 1.5 to 5 times higher than on
  one shard holding the same documents (the cause is not examined; the statistics used are not the whole
  index's), so the strata engine finds the same documents and counts but not the same weights there; on one
  shard they agree, and plain terms, SYNONYM and the other operators agree to 1e-9 on one and on five.
- Reading an OR inside a MAX the way Xapian does keeps the old rankings for the one place Xapiand could emit it.

10. **Append chains.** `Query::append(op, accumulated, next)` is `next` when the accumulated query is empty by
    its structure (a text that analyses to nothing, which is how every query of the server starts:
    `(<nothing> AND (<nothing> AND term))`); an accumulated query that only misses a segment's terms is a null
    scorer joined by the operator.

## Invariants

| Invariant | Guard |
|---|---|
| Same documents, order, estimate and weights as the Xapian engine for sorted, spied, source and operator searches, on one and five shards | `tests/test_engine_callbacks.cc` (884 checks) |
| An expansion finds the same documents and weighs like one shard holding every document | `same_documents` in the same test |
| A spy sees all matches whatever the window and `check_at_least` | `test_spy`, and the mutant that stops it |
| Values round trip; empty is no value; reserved slot refused | `tests/test_engine_strata.cc`, `values_stored` |
| An append onto an empty query is the next query | `test_appends`, and the mutant that stops it |
| The string tests agree with recursive references | `tests/fuzz_strata_pattern.cc` |

## Differences from the Xapian engine

1. Expansion weights on several shards (above).
2. A term whose documents are all deleted still counts toward `max_expansion` until a merge, and term
   statistics include deleted documents until a merge (the same S2 rule).
3. `max_expansion` exceeded with `Expansion::error` raises `errc::invalid`; the Xapian engine raises
   `errc::internal`.
4. `ELITE_SET` keeps the earlier of terms with equal weight; Xapian's pick among equals follows no order of the
   query.
5. Wildcards and edit distances do not expand terms of the key field (the id terms that lead with the key
   byte are not in the dictionary): refused with `errc::syntax`.
6. A spy's `DocView::did()` is the id across the shards.

## Dependencies that will change

Expansions read the index's key prefix (`in_key_space`, `Plan::key_prefix`) and treat a field and its term as
one flat string (a wildcard's literal prefix is scanned in the dictionary as written). Strata is making
fields first-class and keys leave the term space (strata `design/fields-and-keys`); `expand.cc`
(`in_key_space`, `expand`) is the one place that changes then. Nothing else here reads either.

## Not built

- Term lists (`terms`, `term_details`, `serialise` of a `DocView`) wait for prospect's term-lists plugin (P2,
  `phase4/term-lists`, committed, not landed); `SegmentDocView` has the hook and raises `errc::not_available`.
- Collapse, weight cutoffs, `nearest` and `fuzzy` are still refused (not in S3's row).

## Code

`src/engine/strata/search.cc` (validate, groups, build, collection), `expand.cc`, `pattern.cc`, `doc_view.cc`,
`values.h`; design `strata/docs/design/xapiand-on-strata.md` row S3, D4, G4, G5, G6, G8; operators:
`prospect/docs/mechanisms/query-operators.md`; ledger `docs/status/strata-engine-callbacks.md`.
