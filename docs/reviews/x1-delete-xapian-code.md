> Status: CURRENT (2026-10-29) code review of `phase7c/x1-delete-xapian` at `Xapiand:da2a5ea51` (from master `Xapiand:3b0dc433f`), reviewer: code-reviewer. Probes in `.scratch/review-x1/`.

# Code review: X1, delete the Xapian path

## Verdict: APPROVE WITH CONDITIONS

One small blocker (the Docker entrypoint passes a flag that no longer exists). Everything the prompt asked about holds, with the weakened text oracle and a few leftovers as non-blocking.

## What I ran

| Check | Result |
|---|---|
| Release build (`cmake -G Ninja -DCMAKE_BUILD_TYPE=Release`, `ninja xapiand`, 209 steps) | builds, 0 errors |
| Docs E2E on a Release server (`harness/e2e_capture.sh` with newman 6.2.3, port 18981, fresh data dir) | 371 requests, 641 of 641 assertions, GREEN; equals the ledger and `.scratch/e2e/asserts.json` count |
| `xapiand_test_text`, `_engine_strata`, `_engine_query`, `_engine_document` (Release) | 24523, 9737, 16454, 7734 checks pass; the counts equal the x1-tests ledger |
| `codec_probe.cc` (round trip of 0, -0, +-inf, NaN, +-denorm_min, max, lowest; short input) | bit-exact round trip, short input throws `engine::Error` |
| Runtime probes on a Release server (`--solo` rejected; empty field name; legacy Xapian files on disk) | below |

Not an ASan build: I saw no memory suspicion, and the ledger records an ASan/UBSan run of the E2E and of the 25 ctest targets.

## Blocking

### X1-B1. The Docker entrypoint still passes `--cluster`

`contrib/docker/xapiand/entrypoint.sh:17-18,37,62-66,94`. The script reads `XAPIAND_CLUSTER` (defaulting to the Kubernetes namespace file's content) and appends `--cluster=$XAPIAND_CLUSTER` to the command. The option is gone (`xapiand --solo` answers "The following argument was not expected"; `--cluster` is removed the same way, `src/opts.cc` has no such option). A container started with `XAPIAND_CLUSTER` set, or inside Kubernetes where the namespace file exists, exits at start. The ledger's K3 grep covered `src`, `docs`, `harness` and CI, not `contrib/`. `grep -rniE "cluster|discovery|replica|solo" contrib Dockerfile .github` lists only this file. Fix: drop the `CLUSTER` variable, the two `--cluster` cases and line 94 (about 8 lines). That is "remove, not refuse".

## Answers to the lead's questions

### 1. `src/engine/codec.cc`: not a sortable form, and it need not be

The new form is the raw IEEE-754 bit pattern as 8 little-endian bytes (`codec.cc:30-55`). It is not a sign-flip encoding, so memcmp order does not follow numeric order (`codec_probe`: 50 inversions over the 11 edge values). That does not matter: every user of `engine::serialise_double` ships a value between a callback's two ends and reads it back, never sorts it (`src/aggregations/metrics.h`, `src/multivalue/keymaker.cc:121,131,384,394`, `src/metric_serialise.h:83-91`; I grepped every call site). Sort keys and numeric field values still go through `sortable_serialise` (kept for Phase 8, K4), so nothing outside `codec.cc` decodes the old bytes. The probe round trip is bit-exact for -0.0, infinities, NaN and subnormals. Independence from GPL: the routine is a `memcpy` and a byte loop, the public IEEE-754 layout and nothing else; it shares no structure with a sortable encoding. The header comment in `codec.h:29-31` ("Both ends of a connection must agree") is stale now that there is no connection; see X1-N5.

### 2. The weakened text oracle: does not block, but it is thin

`tests/test_text.cc:216-240` and `tests/fuzz_text.cc` count terms with a position, WILDCARD and EDIT_DISTANCE nodes against the tree's text. A lowering bug that swaps AND for OR, or PHRASE for NEAR, or reorders operands, passes that check. What still stands:

- The 706 recorded decisions still pin the parser's own tree and the error texts (`test_text.cc:177-210`).
- `tests/test_engine_query.cc:338-349` runs `parse_text` for a dozen operator shapes against a three-document shard (AND, NOT, `+`, `-`, wildcard, fuzzy).
- `src/engine/text.cc` changed by 2 lines in X1, so the lowering that Xapian once checked is unchanged code.

A stronger oracle exists without Xapian: print the lowered `engine::Query` in the parser's description grammar (operator, operands with `@pos`) and compare it with the tree text, or evaluate both over a model corpus. The 706 recorded descriptions plus the 5000 larger rows make the first one cheap. File as a follow-up (X1-N1); it does not block because the code under test is unchanged and the parser tree is still checked.

### 3. Deleted or rewritten tests

I read every row of the table in `docs/status/x1-tests.md`. The Xapian-comparison tests are properly replaced by models (`test_matches_model`, `test_field_terms_match_model`, the callbacks model), and the counts reproduce. Coverage that went with Xapian and has no stand-in:

- `test_views_do_not_leak` (no live-view counter on strata): view leaks are covered only by the ASan run. A strata `DocView` leak would pass the unit suite.
- `test_view_throws_engine_errors` (every `DocView` method throws `errc::closed` after close): the strata fenced and cancel tests cover a different path. Reads through a view of a closed shard are not tested on strata.
- `test_engine_replay` plain-handle and transaction visibility tests: strata handles share the writer's index, so the semantics changed; `test_engine_strata` covers commit-on-drop and cancel.
- `test_adapter_bounds` (posting source max-weight and term-frequency bounds): ranking by a source's weight stays in `test_posting_source`, the bounds do not.

None is a strata behavior the production path depends on in a way I could break with a probe; I list them as X1-N2. The mutants in the ledger (`?` wildcard, `scan` boundary, `wdf_inc`) are real tests of strata behavior.

### 4. `Document::field("")`: pre-existing, and the HTTP path hides it

`src/engine/strata/document.cc:524` (`name.empty() || name == KEY_FIELD` is refused) is not in X1's diff (`git diff 3b0dc433f..HEAD -- src/engine/strata/document.cc` is one comment). It was there on master with strata; the Xapian engine accepted an empty name, so it surfaced when the tests stopped running Xapian. Over HTTP, `PUT /e1/1 {"":"x","a":1}` answers 204 and stores only `a` (probe `curl` sequence in the runtime section): the schema layer drops the empty key before the engine sees it, so no user write reaches the refusal. Not X1's; fix the builder to refuse at `field("")` (X1-N3).

### 5. What remains

- `Shard::_local` / `is_local()` is always true (`src/database/shard.cc:353,411,494,704,731-760,799,832,971,1029`; `pool.cc:169,252,342,639,686`). The `if (local)` branches are dead scaffolding, not a refusal: nothing answers "refused" or is disabled. X2 removes them per the ledger.
- `num_replicas_plus_master` and `number_of_replicas` stay as an accepted, stored setting (`src/index_resolver_lru.cc:58-69,101-142,195-262,343-376,540-570`), with `settle_replicas` still trimming nodes per shard against a one-entry node table. Harmless on one node, and 7b rebuilds it; but it is stored state that does nothing, which the remove-not-refuse rule would delete. Recorded as the lead's call (X1-N4); the docs say so.
- No discovery, Raft or replication code remains in `src` (grep for `discover|raft|replicat` hits only two comments, `src/main.cc:1086` and `src/database/shard.cc:795`).
- `harness/profile_bulk.sh:118-119` and `harness/bodydiff.py:18` mention Xapian in comments and regexes; `harness/strata_write_check.sh:4` says the registry stays Xapian. Stale text only.

### 6. Build and runtime

Release build and the docs E2E pass (table above). Two runtime probes outside the ledger:

- Upgrading over a Xapian data directory is silent. I put `iamglass` and `postlist.glass` files into the registry shard (`.xapiand/indexes/.__1`) and into an index's shard directories, then started the server. It starts normally ("Ready to Rock"), writes a fresh `strata/` directory beside the old files, leaves the Xapian files in place, and the old indexes read as `404 No such database`. No data is destroyed, but nothing tells the operator why their indexes are gone. The changelog says Xapian-format databases are not read. A one-line startup error naming `iamglass` would be a safety check, not a retained feature (X1-N6).
- `PUT` of `-0.0` into a field the schema already typed integer answers `500 "Value of FLOAT is not a valid NEGATIVE_INTEGER"` where a 400 is expected. `src/serialise.*` is untouched by X1 and `schema.cc` loses only 80 deleted lines; I believe it pre-exists, but I did not run master to attribute it (X1-N7).

## Non-blocking notes

- X1-N1. Stronger lowering oracle (see 2).
- X1-N2. Coverage with no stand-in (see 3).
- X1-N3. Builder should refuse `Document::field("")` (see 4).
- X1-N4. Delete `number_of_replicas` storage and `settle_replicas` with X2, or keep by the lead's choice (see 5).
- X1-N5. `src/engine/codec.h:29-31` comment still talks about connections; say "between a callback's results across shards".
- X1-N6. Detect an `iamglass` file at open and fail with a message (see 6).
- X1-N7. `-0.0` into an integer field is a 500 (see 6); attribute on master.
- X1-N8. The design's X1 criterion says "566 of 566" E2E assertions; the branch reports 641 of 641 and the lead's own table compares with 649 on master. The 8 missing assertions are accounted for page by page in the ledger. The design number is stale; align it.
- X1-N9. `ENGINE_CENSUS`, `fuzz_strata_*` and `strata_check_tool` were compiled only for syntax; `test_engine_census` expectations after the field rename are not re-derived (ledger "Not run").
- X1-N10. The ledger's finding 1 (prospect merge with `--shards 1`) is outside X1; not counted.

## Probes kept in `.scratch/review-x1/`

`codec_probe.cc` (and its binary `codec_probe`), `configure.log`, `build.log`, `cfg2.log`, `tbuild.log`, `d1.log`, `d1b.log`, `d3.log`, `e2e_capture.log`. The runtime probes were `curl` sequences, recorded above. The Release build directory and data directories were removed after the run.

## Conditions

1. Fix X1-B1 (remove `--cluster` from `contrib/docker/xapiand/entrypoint.sh`); a re-check can be a read.

## Not run

ASan or TSan builds (no memory or concurrency suspicion; the ledger records the ASan/UBSan run); the other 21 ctest targets and the fuzz targets; Debug build; CI on any platform; the Docker image; master side by side (for X1-N7); a mutant of the text lowering (the weakness is by reading `test_text.cc:216-240`).
