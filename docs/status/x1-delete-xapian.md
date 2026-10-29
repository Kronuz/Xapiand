> Status: CURRENT (2026-10-29) Phase 7c step X1, branch `phase7c/x1-delete-xapian` (from master `Xapiand:3b0dc433f`); workstream x1-delete-xapian, developer. Local only, not pushed.

# X1: delete the Xapian path

## State

The Xapian engine and the cluster layer are deleted; Release and Debug build, the unit tests pass, and the docs E2E is green on a Release and an ASan/UBSan server. Two findings need the lead (below). The unit-test conversion has its own ledger, `docs/status/x1-tests.md`.

## Done criteria (set by the lead)

| # | Criterion | Evidence |
|---|---|---|
| K1 | `src/xapian/` gone; no Xapian include, link, CMake option or dependency | `src/xapian/` (180,386 lines), `src/engine/xapian/`, `fix_xapian_includes.py`, `docs/dev/xapian-fork.md` removed with `git rm`; the Xapian library target, `XAPIAN_ASSERTS`, `XAPIAN_LIB_BUILD`, ICU (`find_package`, `USE_ICU`), perl/Tcl in CI, Docker, the Homebrew formula and FreeBSD, and `config.h.in`'s Xapian block are gone. `grep -rniP 'xapian(?!d)' src CMakeLists.txt cmake config.h.in` lists only `src/engine/strata/search.cc:157,196` (`p::weight::bm25_xapian`, prospect's name for the weight formula). `src/sortable_serialise.*` is not opened (K4). |
| K2 | The adapter and the engine switch gone | `engine::Kind`, `kind_of`, `default_kind`, `parse_kind`, `src/engine/kind.*`, `XAPIAND_ENGINE`, `Shard::kind()`, `Shard::native<T>()` removed; `engine::open_local(path, ...)`, `Document()` and `Document::unserialise(bytes)` take no engine; the registry (`.xapiand/...`) is a strata index. The adapter's three live duties moved: `engine::prepare` (src/engine/search.cc), `engine::serialise_double` (src/engine/codec.cc: a plain 8-byte little-endian form; the old form was Xapian's GPL routine), and error kinds (`engine::Error(errc, text)` replaces the `Failure<>` types). |
| K3 | Remote, replication, election and cluster-only code removed | Deleted: `src/server/{remote,replication}_protocol_*`, `discovery.*`, `raft_delegate.h`; `XapiandManager`'s cluster state (Raft commands, JOINING/WAITING states, `.xapiand/nodes`, `load_nodes`, `new_leader`, the readiness checks for the deleted services, `trigger_replication`), the `db_updater`/`schema_updater` debouncers, per-node revisions and `pending_revision` in the pool, `DB_REPLICA`/`DB_TRIGGER_REPLICATION`, `send_update` through the WAL, the options `--solo --cluster --discovery-* --xapian-port --replica-port --primary-node --matchers --flush-threshold` and their thread-pool sizes, the `replicas` log category, the replication and remote metrics, `Node` leader/primary/quorum/touch_sender/ports/serialise, the `cluster` and `flume` fetches, `cluster_name`/`nodes` in `GET /`; docs pages `clustering`, `reference-guide/cluster`, `kubernetes`, `indexes/replication`, `building-xapian` and their images; harness `cluster_check.sh`, `cluster_bench.sh`, `discovery_sniff.py`, `proxy`, `engine_migration_check.sh`, `stem_diff.py`, `seam_check.sh`, `fields_identity.py`, `strata_callbacks_check.py`. Also removed because only Xapian served them: `_cjk_words` (schema property, ICU word breaks, `text::WORD_BREAKS`). What remains is listed under "Cluster code that remains". |
| K4 | `sortable_serialise` stays if used | Kept, unopened. Users: `serialise.h` (numeric, date and time fields), `database/utils.h` (`key_placement_hash`), `database/shard.cc` (revision and docid), `database/handler.cc`, `database/schemas_lru.cc` and `index_resolver_lru.cc` (versions), `server/search_views.cc`, and `engine/strata/shard.cc` (the version slot). Phase 8 replaces it. |
| K5 | Builds, unit tests, docs E2E, one ASan/UBSan build | Release and Debug build with no errors. Unit tests: 25 of 25 ctest targets pass on Debug (`docs/status/x1-tests.md` has counts, deleted tests and mutants). Docs E2E on strata, Release server: 371 requests, 641 of 641 assertions (master: 377 requests, 649 of 649). The 8 missing assertions are the removed pages exactly: `replication.md` (5: shards-and-replicas request, 3 success, "partitioned into 3 shards", "was indexed") and `cluster.md` (3: "cluster name is Xapiand", "lists at least one active node", "returns Prometheus metrics"). Two `_cjk_words` expectations were removed from inside larger `pm.test` blocks (`tests/schema/define-prefix.md:54`, `define-slot.md:50`), so no assertion disappeared for them. ASan/UBSan (Homebrew LLVM 22, `RelWithDebInfo`): docs E2E 641 of 641 with no sanitizer report; the 25 ctest targets pass under ASan/UBSan (`ctest -j3`, `detect_leaks=0`, 25 of 25, no sanitizer report). |
| K6 | Changelog, extraction ledger, README, ARCHITECTURE, AGENTS | Changelog `[Unreleased]` has four `Removed` bullets; one row added to `docs/dev/extraction-ledger.md`; README, ARCHITECTURE (diagram re-rendered), AGENTS, the man page, `docs/src` pages and `docs/dev/testing.md` rewritten for one node. |

## Done

- Source, CMake, CI, docs and harness conversion: `Xapiand:5a517b052`, `Xapiand:6dcbb3686`, `Xapiand:1a0964e2d`.
- Extraction ledger row: `Xapiand:621dd690b`.
- Unit tests converted to strata (agent ledger `docs/status/x1-tests.md`): `Xapiand:1848ed81e` to `Xapiand:6dfd8793a`.

## Decisions made (the lead may overrule)

1. `--shards` (default 5) stays: it sat inside the clustering block of the options but sharding within one node is unchanged. Without it the default fell to 1 shard.
2. `number_of_replicas` stays an accepted, stored index setting with no effect on one node (docs say so on the create-index page); `opts.num_replicas` defaults to 0 (it was 1). The resolver still keeps one node name per shard.
3. `GET /` loses `cluster_name`, `_id` and `_version` (they came from the `.xapiand/nodes` document) and keeps `name`.
4. `?volatile` stays (it opens the writer handle; not cluster-only).
5. `engine::serialise_double` changed wire form (transient: aggregation merge and key-maker round trips only).

## Cluster code that remains, and why

- `src/node.*`: the local node's identity (name, address, HTTP port) and a one-entry node table; `Endpoint` carries a node name; `IndexSettings` keeps `shards[i].nodes` (the local node) and `num_replicas_plus_master`. These are index-settings plumbing for sharding and 7b rebuilds them.
- `.xapiand/indexes` (the index registry) now lives in a strata index; `.xapiand/nodes` is no longer created.
- `Shard::_local`/`is_local()` and the `local` flag at the engine seam are always true; the seam rewrite (X2) removes them.
- `XapiandManager` keeps the READY/RESET/BAD states and `cluster_name` as a metrics label.
- Discovery and Raft: nothing remains. They served only the deleted services in this release, and 7b rebuilds membership.

## Findings for the lead

1. **Strata merge fails on the registry with one shard.** `xapiand --shards 1`, then `PUT /twitter/user/Kronuz` and `PUT /twitter/tweet/1` answers `500 strata: a merge's output is corrupt: strata: corrupt segment: field directory`, the writer is fenced and the next write says "the strata index was replaced". Reproduces on Release and Debug; the same sequence passes on master with the registry on Xapian. With the default 5 shards the docs suite passes, so the registry now depends on the shard count. The first shutdown of a fresh node also logs "strata: the commit on closing .xapiand/indexes/strata failed: the writer is fenced". Not investigated further; needs the strata owner.
2. `Document::field("")` is accepted but the shard refuses the write (x1-tests finding).
3. Weakened test: "lowering equals the parser's tree" compares leaf counts only (x1-tests).

## Left alone (history, not described as current)

`docs/design`, `docs/reviews`, `docs/status` (other ledgers), `docs/perf`, `docs/gates`, `docs/mechanisms` and the rest of `docs/dev` describe Xapian and the cluster as they were; they need a labelling pass by the lead. `harness/profile_bulk.sh` still has a regex group for Xapian frames. `docs/src/content/docs/copyright.md` keeps its Xapian licensing text.

## Next

Nothing; push is the lead's. Build directories were removed; `.scratch/` holds the baseline and E2E reports.

## Blocked on

Nothing.
