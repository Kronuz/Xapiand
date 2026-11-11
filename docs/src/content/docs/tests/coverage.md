---
title: "Coverage"
---

The documentation site is the end-to-end suite: every request block on a page
runs against a fresh node, in sidebar order, and the `pm.test` assertions under
it check its answer (`docs_to_postman.py` builds the collection, the `e2e` and
`tls` layers of `harness/run_all.sh` run it over HTTP and HTTPS). This page maps
each feature the node serves to the page that documents it and the tests that
check it, and lists what no request covers, with the reason.

## Coverage

"Docs E2E" means the page's own requests and assertions. Unit tests are under
`tests/`, the harness checks under `harness/`.

### Documents

| Feature | Page | Tests |
|---|---|---|
| `PUT`, `POST` of a document, minted IDs, IDs `0` and `"0"` | [Index](/Xapiand/reference-guide/documents/index-api) | Docs E2E; `tests/test_node.cc` (`test_document_ids_are_themselves`); `harness/strata_write_check.sh`, `harness/id_terms_check.py` |
| `GET` of a document, `selector`, `comments` | [Get](/Xapiand/reference-guide/documents/get-api) | Docs E2E |
| `HEAD` of a document | [Exists](/Xapiand/reference-guide/documents/exists-api) | Docs E2E (an existing document) |
| `UPDATE`, `PATCH`, `UPSERT` | [Update](/Xapiand/reference-guide/documents/update-api) | Docs E2E; `tests/test_node.cc` |
| `INFO` of a document | [Info](/Xapiand/reference-guide/documents/info-api) | Docs E2E |
| `DELETE` of a document | [Delete](/Xapiand/reference-guide/documents/delete-api) | Docs E2E; `tests/test_node.cc` (`test_a_versioned_delete_checks_the_document_its_key_names`) |
| `STORE`, blobs by content type | [Store](/Xapiand/reference-guide/documents/store-api) | Docs E2E |
| Lua scripts on writes | [Scripting](/Xapiand/reference-guide/documents/scripting) | Docs E2E; `tests/test_lua.cc`, `tests/fuzz_lua.cc`; `harness/lua_bounds.py` |
| `?version` on every write, `409` with `_version` | [Versioning](/Xapiand/reference-guide/documents/versioning) | Docs E2E; `tests/test_node.cc` (`test_a_version_is_a_precondition_of_every_write`, `test_writers_expecting_one_version_apply_once`, `test_versioned_writers_match_a_sequential_model`); `tests/test_engine_strata.cc` |

### Indexes

| Feature | Page | Tests |
|---|---|---|
| `PUT` of an index, settings, shards, name rules | [Create](/Xapiand/reference-guide/indexes/create-api) | Docs E2E; `harness/path_boundary_check.sh`; `tests/fuzz_index_path.cc` |
| `GET`, `HEAD`, `INFO` of an index | [Get](/Xapiand/reference-guide/indexes/get-api), [Exists](/Xapiand/reference-guide/indexes/exists-api), [Info](/Xapiand/reference-guide/indexes/info-api) | Docs E2E |
| `DELETE`, `CLOSE`, `OPEN` of an index (`501`) | [Delete](/Xapiand/reference-guide/indexes/delete-api), [Close](/Xapiand/reference-guide/indexes/close-api) | Docs E2E; `harness/removed_features_check.sh` |
| `DUMP`, `RESTORE` | [Backups](/Xapiand/reference-guide/indexes/backups) | Docs E2E; [Restore](/Xapiand/tests/restore/restore); `harness/restore_schema_check.sh`, `harness/restore_stream_check.sh`, `harness/cli_roundtrip_check.sh` |
| `WAL`, replay after a crash | [WAL](/Xapiand/reference-guide/wal) | Docs E2E; `tests/test_wal_replay.cc`, `tests/test_engine_replay.cc`; `harness/wal_recovery_check.sh`, `harness/wal_replay_check.sh`, `harness/wal_replay_agg_check.sh`, `harness/engine_crash_check.sh` |
| Index patterns over several indexes | [API](/Xapiand/reference-guide/api) | Docs E2E (`SEARCH`, `COUNT`); `tests/test_index_pattern.cc`, `tests/fuzz_index_pattern.cc`, `tests/test_wide_search.cc`; `harness/wide_search_check.py` |

### The node

| Feature | Page | Tests |
|---|---|---|
| The verbs, `OPTIONS`, `405` and `400` answers, statuses | [API](/Xapiand/reference-guide/api) | Docs E2E |
| `COMMIT`, `CHECK`; `FLUSH` and `QUIT` without `--admin-commands` (`405`) | [API](/Xapiand/reference-guide/api#administrative-commands) | Docs E2E |
| The limits (patterns, aggregation depth, body nesting) | [API](/Xapiand/reference-guide/api#limits) | Docs E2E; `harness/query_depth_check.py`, `harness/terms_cap_check.py` |
| `GET /`, `GET /:metrics` | [API](/Xapiand/reference-guide/api), [Options](/Xapiand/options) | Docs E2E; `tests/test_root_routes.cc` |
| HTTPS | [Options](/Xapiand/options) | the `tls` layer (the whole E2E over HTTPS); `harness/tls_check.sh` |
| Graceful stop, signals | [Options](/Xapiand/options) | `tests/test_node.cc`; `harness/graceful_shutdown_check.sh`, `harness/signal_check.sh` |
| Refused options and data directories of earlier releases | [Upgrading](/Xapiand/upgrade) | `harness/removed_options_check.sh`, `harness/xapian_data_guard_check.sh`; `tests/test_xapian_data_guard.cc` |

### Schemas

| Feature | Page | Tests |
|---|---|---|
| Field types (boolean, keyword, text, numeric, date and time, geospatial, UUID) | [Field Types](/Xapiand/reference-guide/schemas/field-types) and its pages | Docs E2E; [Check Datatypes](/Xapiand/tests/datatypes/check-data-types), [Check DateType](/Xapiand/tests/datatypes/date-type), [UUID Field](/Xapiand/tests/schema/uuid-field); `tests/test_serialise.cc`, `tests/test_geospatial.cc`, `tests/test_engine_fields.cc` |
| Dynamic typing and detection | [Dynamic Typing](/Xapiand/reference-guide/schemas/dynamic-typing) | Docs E2E; `harness/datetime_detection_check.py` |
| Explicit and cast types | [Explicit Types](/Xapiand/reference-guide/schemas/explicit-types), [Casting Types](/Xapiand/reference-guide/schemas/casting-types) | Docs E2E |
| Namespaces, dynamic field names | [Namespaces](/Xapiand/reference-guide/schemas/namespaces), [Dynamic Field Names](/Xapiand/reference-guide/schemas/dynamic-field-names) | Docs E2E; [Namespace](/Xapiand/tests/namespace/namespace) |
| Foreign schemas | [Foreign Schemas](/Xapiand/reference-guide/schemas/foreign-schemas) | Docs E2E; `harness/restore_schema_check.sh` |
| Accuracy, indexing mode, meta-data | [Accuracy](/Xapiand/reference-guide/schemas/accuracy), [Indexing Mode](/Xapiand/reference-guide/schemas/indexing-mode), [Meta-data](/Xapiand/reference-guide/schemas/meta-data) | Docs E2E; [Global terms](/Xapiand/tests/globalterms/global-terms), [Define Prefix](/Xapiand/tests/schema/define-prefix), [Define Slot](/Xapiand/tests/schema/define-slot); `harness/numeric_range_check.py`; `tests/test_range_terms.cc`, `tests/test_generate_terms.cc` |
| Null and empty values, arrays, objects | the field type pages | [Null type](/Xapiand/tests/nulltype/null-type), [Replace Value by Null](/Xapiand/tests/nulltype/replace-null), [Empty Value](/Xapiand/tests/schema/empty-value), [Arrays](/Xapiand/tests/datastructures/arrays), [Objects](/Xapiand/tests/datastructures/objects) |

### Search

| Feature | Page | Tests |
|---|---|---|
| `SEARCH`, `COUNT`, `limit`, `offset`, `sort`, `q`, `selector`, `collapse` | [Search API](/Xapiand/reference-guide/search/search-api) | Docs E2E; `harness/strata_search_check.sh`; `tests/test_engine_search_strata.cc`, `tests/test_engine_sort.cc` |
| The query DSL: leaf, compound and logical queries, ranges, casts | [Query DSL](/Xapiand/reference-guide/search/query-dsl) and its pages | Docs E2E; [Range Search](/Xapiand/tests/schema/range-search), [Fuzzy](/Xapiand/tests/querydsl/fuzzy), [Partial](/Xapiand/tests/querydsl/partial), [Wildcard](/Xapiand/tests/querydsl/wildcard); `tests/test_engine_query.cc`; `harness/query_depth_check.py` |
| The query string grammar | [Text Queries](/Xapiand/reference-guide/search/query-dsl/leaf-queries/text-queries) | Docs E2E; `harness/boolean_query_check.py`; `tests/test_fieldparser.cc`, `tests/test_text.cc`, `tests/fuzz_text.cc` |
| Metrics and bucket aggregations | [Aggregations](/Xapiand/reference-guide/search/aggregations) and its pages | Docs E2E; `harness/agg_buckets_check.py`, `harness/terms_cap_check.py`; `tests/test_metric_serialise.cc` |
| Spatial search | [Spatial Search](/Xapiand/tutorials/spatial-search) | Docs E2E; `tests/test_geospatial.cc` |

## Not covered by a request

| Feature | Reason |
|---|---|
| `FLUSH` and `QUIT` with `--admin-commands` | The E2E node runs without the option, and `QUIT` would stop it; their `405` is covered. |
| `HEAD` of a missing document | It answers `404` with a JSON body, which newman cannot parse; the page states the `404`. |
| A body over 16 MiB (`413`), streamed blobs up to 4 GiB | Too large for a docs request. |
| 4,096 clauses or 100 levels of a query | The request would be pages long; `harness/query_depth_check.py` covers the depth. |
| `Retry-After` with `503` and `429` | Needs a loaded node. |
| `check_at_least`, `volatile`, `spelling`, `synonyms`, `icase` | No effect a request can observe; the last three are read and ignored. |
| `--max-search-indexes` and the other startup options | The harness starts the node with its own options; [Options](/Xapiand/options) documents them. |
