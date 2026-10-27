# Xapiand code review: engine query (X3 step 1)

- Date: 2026-10-26.
- Branch: `phase0.5/engine-query`, head Xapiand:c281e9618, three commits over local master Xapiand:201414347 (801f16f5c, 868af2635, c281e9618). Local only, nothing pushed.
- Tier: shared API (`engine::Query` and the callback bases are the seam strata implements), larger change (23 files, +2503 -726).
- Reviewed against: `strata/docs/design/xapiand-on-strata.md` (D4, D13, row X3), `docs/status/engine-query.md`, `docs/status/engine-core.md`, `docs/reviews/engine-core.md`. `docs/reviews/calibration.md` does not exist in this repo.
- Reviewer: code reviewer, fresh context. Never opened `src/xapian/`, `src/sortable_serialise.*`, or `src/server/remote_protocol_views.cc` (diffed with the exclusions; only its diff stat, 5 insertions and 4 deletions, was seen). Xapian was used only through its public API, the way the wrapper code uses it.

## Verdict: APPROVE WITH CONDITIONS

Does not land until CR1 is fixed (blocking). CR2 and CR3 are cheap and belong in the same fix round. Everything else is a non-blocking note or a ruling.

The translation itself is faithful: operator shapes, `append` resolution, wildcard and fuzzy parameters, text parser setup, range bounds and error types match master on everything I could run (301-answer corpus identical, 22091 unit checks, 14 of 14 unit tests under ASan+UBSan). The defect is in ownership: every translation of a source query leaks.

## Blocking

### CR1. Every translated source query leaks the adapter and the host source (BLOCKING)

`src/engine/xapian/query.cc:164-165`:

```
case QueryKind::source:
    return Xapian::Query(new PostingSourceAdapter(node->src->clone()));
```

Master built these with `new MultipleValueRange(...)` and `Xapian::Query(mvr->release())`. `release()` is how Xapian's public API hands a heap-allocated `PostingSource` to a `Query`. Without it Xapian does not take ownership of the object it was given: it clones the source (the adapter implements `clone()`) and owns only the clone. The original adapter, and the `engine::PostingSource` clone it owns, are never freed.

Evidence (probe `.scratch/probes/probe_eq.cc`, mode `leak`, a counting `engine::PostingSource` translated and searched 100 times, then the `Query` destroyed):

```
leak: after 100 translate+search (query still alive) live=101 made=301
leak: after Query destroyed live=100 (expect 0) made=301
```

One leaked adapter and one leaked source per translation. Mode `rss` translates a real `MultipleValueRange` 300000 times: maxrss 40 MB to 125 MB, 288 bytes per translation under ASan (smaller unsanitised, still linear). Every range, date, time, timedelta and geo search on a slot field pays it, and `get_mset` with `nearest` or `fuzzy` translates the same `Query` again in `get_rset` (3 translations, 3 leaks per search). A node serving 1000 range searches per second leaks on the order of 10 GB a day.

Why nothing caught it: macOS ASan does not run LeakSanitizer, and `test_engine_query` never counts live sources. The ledger's proof item 5 ("0 AddressSanitizer reports") cannot see leaks on this host.

Fix (probe mode `leakfix` shows both end at `live=0`): construct the adapter on the stack and pass its address (`PostingSourceAdapter adapter(node->src->clone()); return Xapian::Query(&adapter);`, which clones), or `auto* a = new ...; return Xapian::Query(a->release());`. Add a regression test with a live-instance counter in the engine source, asserting zero live after the `Query` and every `Xapian::Query` copy are gone. The `registry` paths (`register_posting_source` passes a temporary by const reference; `unserialise_with_registry` returns an owned object for Xapian to adopt) behave as before; the count in `test_registered_sources` is unaffected.

## Conditions (to land, same fix round)

### CR2. Five translation details are unguarded by any test, so ledger criteria 3 and 9 are overstated

Reviewer mutants (`.scratch/rmutants.py`, control R6 "scale factor ignored" is caught, 277 of 22091 checks fail):

| Mutant | Unit test (22091 checks) | 301-answer corpus |
|---|---|---|
| R1 wildcard `PATTERN_SINGLE`/`PATTERN_MULTI` flags dropped from `wildcard_flags` | survives | survives (`.scratch/qrmutants.py`, 0 of 301 differ) |
| R2 adapter `init` no longer calls `set_termfreq_min(0)` | survives | not run |
| R3 adapter drops `max_weight()` (`set_maxweight`) | survives | not run |
| R4 `parse_text` fuzzy limit kind `ERROR` swapped to `MOST_FREQUENT` | survives | not run |
| R5 binary `combine` built as the list form | survives | not run |

R1 is a real behaviour: probe `probe_eq pattern` shows `wildcard("al?ce", ..., PATTERN_SINGLE)` matches `alice` with the flags and nothing without. The corpus does not catch it because `{"name": "al?ce"}` and `br?wn` go through other paths on that data. R2 and R3 change estimates and pruning bounds (`get_matches_lower_bound`, max weight) rather than hits, and the geo source relies on R3. R5 may be unobservable in Xapian's description; if so, say so in the test or drop the "kept apart" claim.

Required: tests that fail for R1, R2 and R3 (R1: wildcard against a small database with `?` and `*` patterns; R2: `get_matches_lower_bound` of a filter over a slot every document fills; R3: a source's max weight reaches the matcher, e.g. via `get_max_attained`-style bound or a source whose weight exceeds a wrongly low bound). R4 and R5: add or document.

### CR3. Argument evaluation order decides which field is which in the three `unserialise` functions (CI risk on Linux)

`src/multivalue/range.cc:405-410, 481-486, 556-561` (the geo source reads its slot into a local first and is fine) build the source with `unserialise_length(*it), std::string(*(++it)), std::string(*(++it))`: the order of evaluation of function arguments is unspecified. Clang (the only compiler I ran) evaluates left to right; GCC commonly evaluates right to left, which would swap slot, start and end. The expression is unchanged from master (so a remote range search on a GCC build was already at risk), but the branch adds `test_registered_sources`, which rebuilds these sources through `unserialise` and would then fail on Linux CI. I could not run GCC here. Fix: read the three fields into locals in order, then construct.

## Rulings

### R-A. PostingSource as a value-slot filter: accept it

The three real sources (`MultipleValueRange`, `MultipleValueGE`, `MultipleValueLE`, and `GeoSpatialRange` makes four) are all `slot` + predicate + weight, and the Xapian adapter reproduces `next`, `skip_to` and `check` exactly like the code it replaced (I read the old and new bodies side by side; the registry, clone and serialise round trips are covered by `test_registered_sources`, and the corpus shows no answer change).

For strata the filter is the easier contract to implement, not the harder: `next` is a scan of the slot column calling `accept`, `skip_to` and `check` are a column lookup by docid then `accept`, `weight` scores a hit, and `max_weight` feeds pruning. A general `next/skip_to/check` cursor would force strata to expose docid iteration for a source it cannot see into. Locking the filter shape does not force a seam rewrite at the S-steps: a cursor-style source is a new base (or a new node kind) beside it, and every caller is ours. The lead's leaning is right.

Three small additions to the contract now, none of them breaking:

1. State in `callbacks.h` that `accept`, `weight` and `clone` are const and must be thread-safe (the `Query` shares one `shared_ptr<const PostingSource>` across translations and, later, shards).
2. State what `accept` does on a value it cannot decode (CR5).
3. There is no selectivity estimate hook (Xapian gets only `set_termfreq_min(0)`); strata's planner will want an optional `estimate`. Additive, default "unknown".

### R-B. `SerialiseList::size()` no longer `noexcept`: benign

The only callers that evaluate `size()` on a list built from untrusted bytes are the four `unserialise` functions (`range.cc` x3, `geospatialrange.cc:150`) and `AggregationMatchSpy::unserialise` (`aggregations.cc:373`); all five sit inside `try { ... } catch (SerialisationError)`. No `noexcept` function or destructor calls it (`git grep` over `src` outside the GPL exclusions). The fix is correct and the garbage test exercises it. I did not find a hot-path caller; the removal costs nothing measurable.

### R-C. Lifetimes and ownership

1. `Query` owns sources by `shared_ptr<const PostingSource>`; the Xapian translation clones per call; engine-side adapters own their source (`unique_ptr`). `KeyMakerAdapter` and `MatchSpyAdapter` offer a borrowing constructor whose "must outlive" rule is only a comment. Reasonable, but nothing in step 1 uses them (see CR8).
2. Remote: `Registry` clones prototypes (`register_posting_source` takes a const reference), so the temporary prototype dies safely; `unserialise_with_registry` returns an owned adapter for Xapian to adopt. `rebuild()` maps `engine::Error` to `Xapian::NetworkError` with the same text; other exceptions pass through as before. Checked by `test_registered_sources` and the garbage test; not exercised across a live cluster by me.

## Non-blocking notes

### CR4. A flat query map now nests one `append` per key, lowering the depth headroom (pre-existing class)

`QueryDSL::process` builds one `Query::append` node per map key, and `to_xapian` and `~Query` recurse on that chain. Probe `.scratch/probes/flatmap.py` (a node, a query object with N distinct fields, ASan+UBSan on the macOS 512 KB pool thread):

| binary | N=700 | N=1000 | N=2000 |
|---|---|---|---|
| base (master) | ok | ok | stack overflow in Xapian's `QueryBranch::get_length` |
| branch | ok | stack overflow in `to_xapian` (query.cc:167) | stack overflow |

So the existing no-depth-cap trap (AGENTS.md) fires at about half the depth on this host. Not a regression in kind, and Linux's 8 MB stacks are far less sensitive, but it argues for a cap in `process` or for building the map case as one flat `combine` of the non-empty parts when the operator is the same. Follow-up.

### CR5. `accept()` on a corrupt stored value terminates the process (pre-existing, now on the seam)

`insideRange(string_view)` is `noexcept` and iterates a `StringList`, which throws `SerialisationError` lazily. Probe `probe_eq garbage` (200000 random stored values): abort at the 404th with `Bad encoded length: length greater than data`. Same on master. Since strata will call `accept` on stored bytes, either catch and treat as "no match" in the three sources (and the geo one) or document that `accept` may throw and have the adapter translate it.

### CR6. `get_mset` translates the same query up to three times

`nearest` and `fuzzy` call `get_rset(query, ...)`, which calls `get_mset(query, ...)`, which calls `to_xapian` again (the text nodes are parsed each time), and the verbose response parses it once more. Cheap to avoid by translating once and passing the `Xapian::Query` down. Multiplies CR1 until that is fixed.

### CR7. `KeyMaker` and `MatchSpy` adapters are scaffolding

Only tests use `KeyMakerAdapter` and `MatchSpyAdapter` and `register_key_maker`/`register_match_spy`; production keymaker and aggregation code still derive from Xapian bases until step 2. The shape is therefore unproven on a real user; revisit the `DocView`-based signatures when step 2 moves `Multi_MultiValueKeyMaker`.

### CR8. Disclosure, accepted

The ledger discloses that a `git grep` printed lines 47-48 and 120-123 of `remote_protocol_views.cc`. A call-site search listing, mechanical edit of four calls and one include, no copying: accepted. I did not read that file.

## What I reproduced

1. Build: ASan+UBSan, RelWithDebInfo, `-DBUILD_TESTS=ON -DLTO=OFF`, x86_64, `.scratch/build-asan` in `Xapiand-wt/review-eq`, `quiet light`, -j4.
2. `ctest`: 14 of 14 pass. `xapiand_test_engine_query`: 22091 checks, the number in the ledger.
3. `qdiff.py` corpus on the branch binary against the developer's `qd-base.json` (master): 301 answers, 0 differ, 253 are 200s.
4. Seam metric (`seam.sh`): 46 files, 889 lines, as stated.
5. Mutants: R1 to R6 on the unit test, R1 on the corpus (table in CR2); the developer's own `mutants.out` is consistent with the 12 listed there.
6. Probes: leak (CR1), pattern and `rss` (CR1, CR2), `garbage` (CR5), `flatmap.py` (CR4).

## Probes and files kept (all in `.scratch/` of the review worktree)

`build-asan.sh`, `rmutants.py`, `qrmutants.py`, `qdiff.py` and `seam.sh` (copies of the developer's), `probes/probe_eq.cc` (modes `leak`, `leakfix`, `pattern`, `rss`, `garbage`, `deep`), `probes/build_probe.sh` (compile and link a probe with the test's flags), `probes/flatmap.py`, `e2e/qd-rev.json`, `e2e/qd-rmut1.json`, `build1.log`, `build2.log`.

## Not run

- Docs E2E (339 requests, 576 assertions) and the two-node `cluster_check.sh`/`cluster_range.sh` (remote registration only read and covered by the registry unit test; the leak and test gaps do not touch them).
- GCC or Linux or FreeBSD builds (CR3 is from reading); LeakSanitizer (not available on this host; the live-counter probe stands in for it); TSan; benchmarks; libFuzzer.
- A fix check: I did not edit the branch, so CR1's fix is shown only on the probe's copy of the adapter lifecycle (`leakfix`).

## Re-check

Head `a808ecc9d` (fix commit `db0be79ad`). Local build `.scratch/build-asan` rebuilt at that head: `xapiand_test_engine_query` 22395 checks pass under ASan and UBSan. Nothing edited in the source tree (mutants were applied and restored; `git status` clean).

| Item | Ruling | Evidence |
|---|---|---|
| CR1 | CLOSED | `src/engine/xapian/query.cc:197-199` builds the adapter and passes `adapter->release()` to the Query. `probe_eq leak`: `live=1` while the query is alive (made=201 over 100 translate-and-search rounds), `live=0` after it is destroyed (was 100 leaked). `leakfix` also 0. Revert (`Xapian::Query(adapter)`): `test_source_ownership` fails 5 checks (`tests/test_engine_query.cc:706, 712, 722, 724, 726`: live count 1/1/2/1/0 off). |
| CR2 | CLOSED | `.scratch/rmutants.py` on the fixed head: R1 wildcard flags dropped fails 3 checks, R2 `set_termfreq_min(0)` removed fails 1, R3 `max_weight` dropped fails 3, R4 fuzzy limit kind swapped fails 2, R6 control fails 277. R5 (binary combine built as a list) survives, as the developer reports; `test_binary_equals_list` (`tests/test_engine_query.cc:796`) now pins that the two forms give the same description and the same hits and weights for every binary op over six operands including empties, so the survival is a stated equivalence, which was the "document" option of the finding. |
| CR3 | CLOSED | `src/multivalue/range.cc:417-419`, `:501-502`, `:583-584`: slot, start and end are `const`/local reads in order, then the constructor call. Read, not testable on this clang (left to right); a reversed-read mutant fails `test_registered_sources` per the ledger. |
| CR4 | CLOSED | `src/engine/xapian/query.cc:167-183` folds the append chain in a loop; `src/engine/query.cc:59-76` (`~QueryNode`) frees uniquely owned children from a list. `probe_eq deep 2000 524288` with the translation only (`NODESC=1`): ok at 2000, 5000 and 20000 (`engine::Query` alone: 200000 built and destroyed). With `get_description()` the probe overflows inside Xapian (`QueryBranch::get_description_helper`) at 2000, and Xapian's own destructor (`SmallVectorI::clear`) overflows at 5000: those are Xapian's recursion, not the seam's. `test_append_chain` covers 600 on 256 KB. |
| CR5 | CLOSED | `probe_eq garbage`: 200000 random stored values, no terminate, hits=19502. `src/multivalue/range.cc:353-372, 443-458, 525-540` (range, GE, LE `insideRange`) and `src/multivalue/geospatialrange.cc:90-122` (`insideRanges`) catch `SerialisationError` and return false; geo `weight` (`:61-85`) returns the no-centroid weight. Revert (range `insideRange` catch rethrows): process aborts (exit -6). `test_corrupt_stored_value` runs 20000 random values per source plus a search over a slot holding one corrupt value. |
| New: `reset(db, shard)` override | CLOSED | See below. |

### New behaviour: bounds now reach the matcher

`src/engine/xapian/callbacks.cc:140-147` overrides `reset(db, shard)`: base reset, then `set_bounds()` (`max_weight()` when non-zero), then `set_termfreq_min(0)`.

From the code, `max_weight() >= weight(doc)` for every accepted document:

- Range, GE, LE (`src/multivalue/range.cc`): `weight()` is a constant 1.0 and `max_weight()` is not overridden, so it is 0.0, which `set_bounds` treats as "not declared" and leaves Xapian's unbounded default. Nothing is declared, so nothing can be pruned wrongly; this is the same state as master.
- Geo (`src/multivalue/geospatialrange.h:85`, `.cc:61-85`): `max_weight()` is `pi - 0`; `weight()` is `pi - min_angle` where `min_angle` starts at `pi` and only decreases to a `distance()` (`acos`, in [0, pi], NaN never wins the `<`). So the weight is in [0, pi] for every value, accepted or not, and a value that does not decode weighs 0.

Probes (`.scratch/probes/probe_bound.cc`, new; output `.scratch/probe_bound_topk.out`):

- `probe_bound srcbound`, 200000 random values per source: Range, GE, LE accepted 126454, 166358 and 171305, weight min = max = 1.0, declared bound 0 (undeclared, unbounded). Geo accepted 132023, weight in [0.000000, 3.137281], declared bound 3.141593: no violation.
- `probe_bound topk`: 2500 documents per shard, 1 and 2 shards, 20 queries, four combinations (`OR`, `AND_MAYBE`, `AND` of a term group with the source, `FILTER` with the source on the left) and limits 1, 3, 10 and 40: 2560 cases. Real bound against unbounded: 0 differences in docids and weights, for every source. A declared tight bound of 1.0 on the three range sources (which the code does not declare today) against unbounded: 0 differences. Control, a declared bound of 1e-6 (far below the real weights): 2260 of 2560 cases differ, so the comparison does detect pruning and the matcher does use the bound.

### New findings

None in the fix. Two notes for the lead, not findings against the branch:

1. The range sources keep weight 1.0 with no declared bound, so they never prune; declaring `max_weight() = 1.0` is safe (probe: 0 differences) if the matcher should skip documents sooner, but that is an optimisation, not part of this fix.
2. `Query::description()` (`src/engine/query.cc`) is recursive, so logging a very deep query still costs stack, as the ledger already says; `probe_eq pattern` prints "expect 2" for `al?ce` in the probe's own text, but the data has only one four-letter-middle match (`alice`; `alce` is shorter), the right count is 1 and `test_wildcard_patterns` asserts the correct set.

### Not run (re-check)

Docs E2E, the two-node cluster scripts, the corpus diff (`qdiff.py`/`qrmutants.py`; the developer reports the 301 answers identical and the changed code is bounds and error paths only), GCC, Linux and FreeBSD builds, TSan, benchmarks, libFuzzer. The CR3 reversed-read mutant and the CR4 recursive-translation revert were not rerun (developer's `mutants2.out` read: both fail). The ASan build tree was left holding the last reverted-mutant build; rebuild it before reuse.

