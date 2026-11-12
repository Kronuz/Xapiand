# phase4-gate: journal

State: working
Quest: .campaign/xapiand/quests/phase4-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1, A3 estimate triples: `check_at_least` and a `{lower, estimate, upper}` triple in the
search result (prospect `include/prospect/searcher.hh`, `shards.hh`), no format change. Rule:
segments are counted exactly, whole, while fewer than `check_at_least` matches are counted (tier 1:
exact whenever matches <= check_at_least); later segments prune, adding lower = hits that entered
the collector from them, upper = their live documents. Tests: tier 1 exact, tier 2
lower <= true <= upper under random k, check_at_least, segmentations and deletes.

## Next

1. A3 estimate triples (Doing).
2. A3 `_percent` at the coordinator, asserted against the percents already in
   `test/fixtures/xapian-ops/ops.jsonl` (unasserted today, `test/operators_test.cc:418`).
3. A5 evidence: point at existing tests; add a get-while-frozen case for the search layout.
4. A4 wiring for `bytes` slots: terms/values buckets fed from values columns through the
   exhaustive visitor; A9 job, one shard against many (indexed, not in-memory partials).
5. Design notes for the format steps (Blocked on: design review), then build after review:
   typed values section (A1), points section on `bkd` (A2), wire forms of filters and
   aggregation partials (A4, A10).

## Inventory at prospect main `0aa68f4` (strata pin `94480d5`)

| Crit | Exists (file:line, tests) | Missing | Size |
|---|---|---|---|
| A1 | Ordered map `include/prospect/ordered.hh:31-45` (i64, u64, f64, BE form; `test/ordered_test.cc`). Typed column codec `include/prospect/docvalues.hh:22-38` (u64/i64/f64/bytes, multi-valued, presence, GCD blocks, skip groups, 128-bit sums; `test/docvalues_test.cc`). Values section wired, bytes-only single-valued (`values.hh:24-33`, `values_writer.hh`; `test/values_test.cc`, `fuzz/fuzz_values.cc`) | Slot table: `declare_slot` operation, `slots-<gen>.kv` in the manifest, typed declarations (strata core: `basic_operation` kinds at strata `index.hh:62` have none; outside this file set). Numeric, `date` and multi-valued columns in the values section (a directory kind other than 3 is refused today: decoder change). Stage-1 type and NaN rejection; typed `V` digest records | L, format + strata core |
| A2 | Points kind reserved: strata `core/format.hh:138` (kind 15), `:153` (bit 32), `:204` (unranked). `Kronuz/bkd` in-memory `tree<D>`, 1-8 dims, interval as 2-D (166 lines; brute-force test, fuzz target, CI) | bkd's persisted form; points plugin and section; points query bitsets from a pool; D7 planning; geo offline rows 1, 2, 4, 5 over the four candidates and the fifth arrangement; `geonames` and the shape corpus; the kept candidate's engine code | XL, format |
| A3 | Scored filter concept `scorer.hh:55-57`, `source_scorer` with `max_weight` `operators.hh:560-605` (`test/operators_test.cc`). Sort keys `sortkey.hh` (components, selection, wire v1, top_k, merge, block bounds; `test/sortkey_test.cc`), standalone. Collapse `collapse.hh` (collapser, shard merge, triple merges `:196-217`; `test/collapse_test.cc`; used by Xapiand `src/engine/strata/search.cc:43`). Exhaustive mode `shards.hh:167-280` (`test/shards_test.cc`) | Estimate triples and `check_at_least` (the result holds an exact count only, `shards.hh:183`; Xapiand forces `count = true`, `search.cc:705`, so it never prunes). `_percent` in the engine (Xapiand computes it host-side, `search.cc:764`). Sort keys computed from slots inside the search, with skip-group block skipping. Range filters with block bounds (need A1). Delete by query (strata core operation evaluated at apply, plus prospect's evaluation) | M + M + L + L (strata) |
| A4 | Collectors `aggregate.hh:387-399` (count, sum, avg, min, max, stats, extended_stats, variance, std_deviation, median, mode; values, terms, histogram, range, filter; nesting by work stacks); exact sums `exact.hh` (`test/exact_test.cc`); splits into 1-64 partials compared byte for byte `test/aggregate_test.cc:268` | Feeding from an index's values (numeric needs A1); the partials' wire form and decoder (fuzz); query-budget charge (D13a); `_terms` over stored values (R-P2) | M-L |
| A5 | Keys section: strata `format.hh:134-135` (`keys`, `key_index`), `keys_plugin` in `search_layout` `reader.hh:144`; F8 tests `test/format13_test.cc:969`. Optional sections: structure bits, F4 tests `format13_test.cc:786` (free and absent, document profile), F9 `:883`. Real-time get by key `test/index_test.cc:159-172`, `:2160-2190` (model through refresh, merge, reopen) | Get by docid of a stored record (strata has `find_docid`'s version only, `index.hh:1736`: strata core); per-mode consistency is Phase 3R's. A prospect case for a get while a buffer is frozen | S |
| A6 | Phase 3 report `docs/gates/phase-3-ratchets.md:109-122`: after merge 14 of 40 miss (1.02-1.24x, 13 against Tantivy), before merge 24 miss (to 1.60x), indexing peak 1.56x budget, allocations per query grow (term class), postings 1.012x and terms 1.123x (D29) | All of it: performance work | XL, perf |
| A7 | Text-class harness `bench/phase2/gate.py`, engines in `bench/engines/` (Lucene 10.4, Tantivy 0.26, Xapian 2.1; PISA quick tier) | Structured corpora (`nyc_taxis` 20M rows, `geonames`); per-engine configurations for range, sort, aggregation, filtered, geo; query sets; four-axis rows | XL |
| A8 | Operator vectors `test/fixtures/xapian-ops/ops.jsonl` (`test/operators_test.cc`), shard vectors `test/fixtures/xapian-shards` (`test/shards_test.cc`); generator `strata-oracle` | Phase 4 vectors (ranges, sort keys, collapse counts, estimates per tier, percent, MatchSpy, geo) in strata-oracle; the job at the gate commit | M, after A1-A4 |
| A9 | In-memory partial splits only (`aggregate_test.cc:268`) | Indexed one shard against many, as a job | S, after A4 wiring |
| A10 | CI ASan/UBSan/TSan (`.github/workflows/ci.yml`, `quest/**` pushes); fuzz `fuzz/fuzz_{values,segment,postings,fielddict}.cc`; `tools/coverage.sh`; fixtures `test/fixtures/*.sx` | A fuzz target and a CPU-hour for every new decoder (typed values, points, slot table, filter and partial wire forms); a fixture per format change; test-parity rows; scale statement (D18) | per item |
| A11 | None | `docs/gates/phase-4.md`, last | S |

Order: no-format steps first (Next 1-4), then the format steps after design review (typed values,
points), geo measured offline before engine code, harness, carried perf rows, gate.

## Proposed split (for the lead)

Too big to land as one. Each part lands alone with its tests:

1. `phase4-query-surface` (prospect, no format): A3's estimate triples, `check_at_least`,
   `_percent`; A5's evidence. This branch.
2. `phase4-typed-values` (strata + prospect, format): A1 (slot table and `declare_slot` in strata
   core; typed, multi-valued columns), A4's wiring and partial wire form, A9, A3's sort-key and
   range-filter wiring. Design review first.
3. `phase4-points-geo` (bkd, prospect, format): A2, geo measurement rows first.
4. `phase4-delete-by-query` (strata core + prospect): A3's last item.
5. `phase4-structured-bench` (prospect harness): A7's corpora and engine configurations.
6. `phase3-carried-rows` (perf): A6.
7. `phase4-gate`: A8, A10's gate rows, A11, and A6, A7, A9 re-run at one commit.

## Blocked on

Lead: the split above, and three strata-core items outside this file set (`include/**` here is
prospect's): the slot table and `declare_slot`, delete by query's operation, get by docid. Options:
a strata quest per item (recommended), or widen this quest to the strata repository.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | <no evidence yet> |
| A6 | <no evidence yet> |
| A7 | <no evidence yet> |
| A8 | <no evidence yet> |
| A9 | <no evidence yet> |
| A10 | <no evidence yet> |
| A11 | <no evidence yet> |

## Result

<Not yet.>

## Rulings I made

- Strata-core items are not built here: the quest's file set is prospect's.
