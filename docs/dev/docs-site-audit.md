# Docs-site audit

> Status: audit of 2026-11-09 (quest `docs-site-audit`, owner ruling D45). Read-only over the site: no page was changed.
> Audited tree: `docs/src/content/docs/` at `docs-site-audit` branch (`8d3a556f2`, master of 2026-11-09), 128 pages, 17,758 lines.
> Source of truth: Xapiand `src/` of that tree, strata (`~/code/Kronuz/strata`), prospect (`~/code/Kronuz/prospect`) and the queryparser library, with their docs.
> Not read: Xapian, `src/sortable_serialise.{cc,h}`, `contrib/python/xapiand-py/xapiand/utils/sortable.py` (GPL).
> No node was started and nothing was built: a claim only a running node settles is under "## Unverified" with its probe.

## Summary

128 pages: current 45, stale 28, wrong 20, missing content 34, delete 1.

Verdicts: `current` every claim true and nothing important missing; `stale` was true of Xapian-era Xapiand or describes an old shape; `wrong` contradicts the code now; `missing content` true but lacks what the code has; `delete` documents only removed features or duplicates another page.

What the audit found, in order of weight:

1. No page documents a removed feature as live (no `_elite_set`, `_cjk_words`, `_terms` over text, cluster, `?nearest`, `#xapian_query`, `cluster_name`): the Xapian purge of the site is done except `welcome.md` ("Highly Available Distributed"), `benchmarks.md` (Xapian-era figures) and `architecture/internals.md` (ten file paths that do not exist).
2. The larger defect is old documentation that was never true of this code: 20 pages are `wrong`, almost all because they were copied from Elasticsearch's docs or written before the code settled (`numeric_detection` default, `geo_point`, `upsert` as a query parameter, version "ignored", fuzzy `_max_expansions` 50, index delete, `DELETE /`, scripts that score queries).
3. Whole behaviors the code has and no page states: the verbs `COMMIT`, `FLUSH`, `QUIT`, `CHECK`, `UPSERT`, `PATCH`, `WAL`; `--admin-commands`; 29 of the 53 options; the search URL parameters and response shape; the query, aggregation and body limits; the queryparser grammar; the term, path and detection rules of schemas.
4. Site infrastructure still describes the Jekyll migration: orphan pages, generated-sidebar header with no generator, `editLink` to a directory that does not exist, `docs/README.md`.

## Page table

One row per page. Detail rows (each stale or wrong claim with `file:line`) follow in "## Pages". "Owned" names the open quests that change the page (see "## Owned by other quests").

| Page | Verdict | Owned |
|---|---|---|
| `index.mdx` | stale | release (name, `/Xapiand/` base) |
| `about.md` | stale | release (name, `/Xapiand/` base) |
| `welcome.md` | wrong | release (name, `/Xapiand/` base) |
| `help.md` | stale | release (name, `/Xapiand/` base) |
| `faq.md` | missing content |  |
| `next.md` | delete |  |
| `contributing.md` | current |  |
| `conduct.md` | missing content |  |
| `copyright.md` | stale | release (name, `/Xapiand/` base); sortable-encoding A8; license-closure A2 |
| `changelog.md` | missing content | release (name, `/Xapiand/` base); sortable-encoding A11; more-like-this A8; barrier-warning A4; release A5 |
| `architecture.md` | stale | release (name, `/Xapiand/` base) |
| `architecture/dependencies.md` | stale |  |
| `architecture/internals.md` | wrong | sortable-encoding (value-encoding row and paragraph) |
| `basics.md` | current |  |
| `installation.md` | current | release (name, `/Xapiand/` base) |
| `building.md` | stale | release (name, `/Xapiand/` base) |
| `upgrade.md` | stale | docs-upgrade-page (`upgrading.md`; the binary prints this page's URL) |
| `options.md` | missing content | barrier-warning A2 |
| `benchmarks.md` | stale | closing-benchmark (file set) |
| `quickstart.md` | stale | release (name, `/Xapiand/` base) |
| `manipulation.md` | current |  |
| `exploration.md` | current |  |
| `tutorials/home.md` | missing content |  |
| `tutorials/spatial-search.md` | current | license-closure A5 (map screenshot) |
| `reference-guide/api.md` | wrong | gate-prep-followups A1 (commands at the root) |
| `reference-guide/documents.md` | missing content |  |
| `reference-guide/documents/delete-api.md` | wrong |  |
| `reference-guide/documents/exists-api.md` | current |  |
| `reference-guide/documents/get-api.md` | stale |  |
| `reference-guide/documents/index-api.md` | wrong |  |
| `reference-guide/documents/info-api.md` | missing content | more-like-this A2 (INFO terms return) |
| `reference-guide/documents/scripting.md` | wrong |  |
| `reference-guide/documents/store-api.md` | missing content | license-closure A1 (Lenna images) |
| `reference-guide/documents/update-api.md` | wrong |  |
| `reference-guide/documents/versioning.md` | wrong |  |
| `reference-guide/indexes.md` | missing content |  |
| `reference-guide/indexes/backups.md` | wrong | docs-upgrade-page (restore of an alpha dump) |
| `reference-guide/indexes/close-api.md` | current |  |
| `reference-guide/indexes/create-api.md` | stale |  |
| `reference-guide/indexes/delete-api.md` | current |  |
| `reference-guide/indexes/exists-api.md` | stale |  |
| `reference-guide/indexes/get-api.md` | stale |  |
| `reference-guide/indexes/info-api.md` | stale |  |
| `reference-guide/wal.md` | stale | gate-prep-followups A5 (`:wal` streams) |
| `reference-guide/schemas.md` | stale |  |
| `reference-guide/schemas/meta-data.md` | current |  |
| `reference-guide/schemas/casting-types.md` | current |  |
| `reference-guide/schemas/namespaces.md` | wrong |  |
| `reference-guide/schemas/field-types.md` | stale |  |
| `reference-guide/schemas/accuracy.md` | missing content |  |
| `reference-guide/schemas/dynamic-field-names.md` | missing content |  |
| `reference-guide/schemas/foreign-schemas.md` | stale |  |
| `reference-guide/schemas/dynamic-typing.md` | wrong |  |
| `reference-guide/schemas/explicit-types.md` | current |  |
| `reference-guide/schemas/indexing-mode.md` | missing content |  |
| `reference-guide/schemas/field-types/boolean-type.md` | missing content |  |
| `reference-guide/schemas/field-types/date-type.md` | stale | sortable-encoding |
| `reference-guide/schemas/field-types/keyword-type.md` | missing content |  |
| `reference-guide/schemas/field-types/numeric-type.md` | stale | sortable-encoding |
| `reference-guide/schemas/field-types/text-type.md` | missing content | cjk-words (`_cjk_words` returns after the release) |
| `reference-guide/schemas/field-types/geospatial-type.md` | wrong | sortable-encoding |
| `reference-guide/schemas/field-types/geospatial-type/htm.md` | stale |  |
| `reference-guide/schemas/field-types/uuid-type.md` | stale |  |
| `reference-guide/search.md` | current |  |
| `reference-guide/search/search-api.md` | missing content | more-like-this A4 (`?nearest`, `?fuzzy`) |
| `reference-guide/search/aggregations.md` | wrong |  |
| `reference-guide/search/aggregations/bucket.md` | wrong |  |
| `reference-guide/search/aggregations/metrics.md` | wrong |  |
| `reference-guide/search/aggregations/bucket/filter-aggregation.md` | missing content |  |
| `reference-guide/search/aggregations/bucket/values-aggregation.md` | stale |  |
| `reference-guide/search/aggregations/bucket/terms-aggregation.md` | stale |  |
| `reference-guide/search/aggregations/bucket/histogram-aggregation.md` | missing content |  |
| `reference-guide/search/aggregations/bucket/range-aggregation.md` | missing content |  |
| `reference-guide/search/aggregations/metrics/count-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/min-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/max-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/sum-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/avg-aggregation.md` | stale |  |
| `reference-guide/search/aggregations/metrics/variance-aggregation.md` | missing content |  |
| `reference-guide/search/aggregations/metrics/std_deviation-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/stats-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/extended_stats-aggregation.md` | current |  |
| `reference-guide/search/aggregations/metrics/median-aggregation.md` | missing content |  |
| `reference-guide/search/aggregations/metrics/mode-aggregation.md` | missing content |  |
| `reference-guide/search/query-dsl.md` | missing content | more-like-this A3 (`more_like_this` clause) |
| `reference-guide/search/query-dsl/compound-queries.md` | missing content |  |
| `reference-guide/search/query-dsl/leaf-queries.md` | current |  |
| `reference-guide/search/query-dsl/compound-queries/filter-operator.md` | current |  |
| `reference-guide/search/query-dsl/compound-queries/geospatial-searches.md` | current |  |
| `reference-guide/search/query-dsl/compound-queries/logical-operators.md` | missing content |  |
| `reference-guide/search/query-dsl/compound-queries/maybe-operator.md` | current |  |
| `reference-guide/search/query-dsl/compound-queries/range-searches.md` | missing content |  |
| `reference-guide/search/query-dsl/leaf-queries/casting-values.md` | wrong |  |
| `reference-guide/search/query-dsl/leaf-queries/match-all-query.md` | current |  |
| `reference-guide/search/query-dsl/leaf-queries/namespace-queries.md` | current |  |
| `reference-guide/search/query-dsl/leaf-queries/simple-queries.md` | wrong |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries.md` | missing content |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/default-operator.md` | missing content |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/fuzzy.md` | wrong |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/love-and-hate.md` | missing content |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/near.md` | missing content | phrase-expansion (later) |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/partial.md` | wrong |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/phrase.md` | missing content | phrase-expansion (later) |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/simple.md` | current |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md` | wrong |  |
| `reference-guide/search/query-dsl/leaf-queries/text-queries/wildcard.md` | missing content |  |
| `tests.md` | missing content |  |
| `tests/comments/ignore-comments.md` | stale |  |
| `tests/globalterms/global-terms.md` | current |  |
| `tests/nulltype/null-type.md` | current |  |
| `tests/nulltype/replace-null.md` | current |  |
| `tests/schema/create-schema.md` | current |  |
| `tests/schema/empty-value.md` | current |  |
| `tests/schema/define-prefix.md` | current |  |
| `tests/schema/define-slot.md` | current |  |
| `tests/schema/uuid-field.md` | current |  |
| `tests/schema/range-search.md` | current | sortable-encoding |
| `tests/datatypes/check-data-types.md` | current |  |
| `tests/datatypes/date-type.md` | current |  |
| `tests/namespace/namespace.md` | stale |  |
| `tests/datastructures/arrays.md` | current | sortable-encoding |
| `tests/datastructures/objects.md` | current |  |
| `tests/restore/restore.md` | current |  |
| `tests/schema/stem-language.md` | current |  |
| `tests/schema/basque-language.md` | current |  |
| `tests/querydsl/fuzzy.md` | current |  |
| `tests/querydsl/partial.md` | current |  |
| `tests/querydsl/wildcard.md` | missing content |  |

## Pages

Format: page, verdict, why; then one line per stale or wrong claim (`L<line>` is the page line): `"quote" -> what is true: file:line`. `file:line` is in the Xapiand tree unless a repository is named. A claim that is true needs no row.

### Top level: home, about, welcome, help, FAQ, community

- `index.mdx` | **stale** | home page sells strata but links Benchmarks (Xapian-era numbers) and "Multi-Types"; names/quick-start owned by release
  - L"Benchmarks →" and "Memory Usage →" -> point at benchmarks.md whose figures were measured before strata (see benchmarks.md)
- `about.md` | **stale** | feature list mostly true; "Multi Types" and MIT claim need check; lacks single-node, TLS, Lua bounds, wide search
  - L"MIT license (a few files carry their own...)" -> true only until the GPL files go: src/sortable_serialise.{cc,h} still in tree (git ls-files src/sortable_serialise.cc) (OWNED sortable-encoding)
- `welcome.md` | **wrong** | calls Xapiand "Highly Available Distributed"; the cluster is removed (single node)
  - L"A Modern Highly Available Distributed RESTful Search and Storage Server" -> one node only: docs/status/behavior-changes-audit.md #3; changelog.md:250 (removed cluster)
  - L"deploying to various environments" -> no deployment page exists (Kubernetes guide deleted, changelog.md:253)
- `help.md` | **stale** | IRC channel on freenode and a Twitter account, both unconfirmed as live
  - L17 "irc.freenode.net/Xapiand" -> unverified that the channel or the account is still used (see Unverified)
- `faq.md` | **missing content** | two true answers and licensing/donation; no answers for the removed cluster, data directory, ports, auth
  - L"`?echo` query param" -> true (src/server/search_views.cc:2223 region)
- `next.md` | **delete** | a three-sentence stub with one link; no content a reader needs
- `contributing.md` | **current** | road ahead text is accurate (single node); no build/test/PR-gate instructions beyond "tests and documentation"
- `conduct.md` | **missing content** | Contributor Covenant with the placeholder contact left in
  - L60 "[INSERT EMAIL ADDRESS]" -> no contact address
- `copyright.md` | **stale** | GPL note is true now and false after sortable-encoding; mentions Python client fork not in packages
  - L19 "src/sortable_serialise.{cc,h} derive from Xapian and are GPL licensed" -> still in tree (OWNED sortable-encoding A8, license-closure A2)
- `changelog.md` | **missing content** | history page; `[Unreleased]` omits removals the audit lists and will gain four entries from open quests
  - L`[Unreleased]` -> lacks the 8 removed cluster options (--replicas, --replicators, --remote-clients, --remote-servers, --replication-clients, --replication-servers, --discoverers, --database-stall-time), that removed options are fatal at startup, the 16 MiB cap on every body, and the alpha.10-section misfiling of the queryparser change (docs/status/behavior-changes-audit.md "Changelog gaps found" 1 to 5)
  - OWNED sortable-encoding A11, more-like-this A8, barrier-warning A4, release A5

### Architecture

- `architecture.md` | **stale** | layers and libraries right (strata via prospect behind src/engine/); link target `src/server/http*` does not exist; no mention of the WorkPool, the control loop or the engine seam census
  - L53 "src/server/http*" -> the HTTP views are src/server/search_views.cc, search_service.cc, http_utils.h, mime_types.cc; the HTTP framework is Kronuz/http (MethodRouter) (src/server/search_views.cc:526)
  - L"Storage ... capped near 34 GB" -> volumes are the DataStorage blob store; strata files are the index (docs/design/xapiand-on-strata.md); cap unverified
- `architecture/dependencies.md` | **stale** | counts and tables out of date: says 59 Kronuz libraries, CMakeLists declares 56 Kronuz + 9 third-party; omits the engine libraries
  - L"59 standalone Kronuz libraries" -> CMakeLists.txt declares 56 `Kronuz/` repositories (git grep -c GIT_REPOSITORY CMakeLists.txt = 68 declares)
  - tables -> missing strata, prospect, queryparser, analysis, snowball, unicode, task, namegen, constexpr-phf, libyaml (CMakeLists.txt:458-1085)
  - tables -> list fantasyname (now namegen), perfect-hash (now constexpr-phf), stash, uinteger_t, times, bloom-filter, lazy, queue, radix-router, which Xapiand no longer declares: changelog.md `[Unreleased]` (57 declared; the libraries pin the rest)
  - L"random: xorshift128+" -> random is xoshiro256++ (changelog.md `[Unreleased]`, node names and random draws)
  - L"Lua 5.4.7, sol2" -> CMakeLists.txt:1034 FetchContent lua; version unverified
- `architecture/internals.md` | **wrong** | "Where Things Live" table names ten paths that do not exist and a GPL-owned value-encoding row
  - L30 "src/database/{wal,data,shard}" -> exist (current)
  - L32 "src/server/http*.{cc,h}, src/url_parser.*" -> no such files; routes are in src/server/search_views.cc:526; URL parsing is Kronuz/url-parser
  - L33 "src/worker.{cc,h}, src/server/base_{client,server}.*" -> no such files; the loops are Kronuz/reactor; src/control_plane.h and src/manager.cc run the control loop
  - L34 "src/booleanParser/" -> removed; text queries are the Kronuz queryparser library via src/engine/text.cc
  - L37 "src/sortable_serialise.*" -> exists until sortable-encoding lands (OWNED); the monotonic-encoding paragraph (L~70) describes it
  - L38 "src/logger.*" -> no such file; src/log.h and Kronuz/logger
  - L55 "`MsgPack` (src/msgpack.h)" -> Kronuz/msgpack library, no src/msgpack.h
  - L85 "src/geospatial/htm.cc" -> no such file; HTM is the Kronuz/htm library (src/geospatial/*.cc is the shapes)
  - L"~2,790 L_* call sites; categories compile to nothing, edit src/log.h" -> unverified count; src/log.h exists
- `basics.md` | **current** | single node, 5 shards (src/opts.cc:43, NUM_SHARDS), `number_of_shards` (src/index_resolver_lru.cc:191) all true
  - L"Near Realtime ... a couple seconds" -> refresh/commit 1 s after the last write (changelog.md `[Unreleased]` autocommit times; src/opts.cc:66 COMMITTER_DEBOUNCE_TIMEOUT 1000) (imprecise, not wrong)

### Install, build, upgrade, configure

- `installation.md` | **current** | Docker, Homebrew, FreeBSD pkg, RPM, DEB all match the release workflows
  - L"brew install Kronuz/tap/xapiand" -> Formula/xapiand.rb exists in Kronuz/homebrew-tap; name changes with release (OWNED release)
  - L"DEB (Debian 13+, Ubuntu 24.04+)" -> .github/workflows/release.yml:243 matches
- `building.md` | **stale** | requirements and options out of date: no OpenSSL 3, no libuuid; CMake minimum; option table incomplete
  - L"CMake >= 3.14" -> cmake_minimum_required(VERSION 3.12) CMakeLists.txt:26
  - L"The only external system dependencies... zlib, libpthread" -> also libuuid (find_package(UUID REQUIRED) CMakeLists.txt:388) and OpenSSL 3 when TLS=ON, the default (CMakeLists.txt:54,404); Dockerfile:42,45
  - L"Build Options" table -> lacks TLS (ON), LUA (ON), UUID_ENCODED/GUID/URN, RANDOM_ERRORS, FUZZ, CRASH_POINTS, ENGINE_CENSUS, CHECK_IO_FDES, CHECK_SIZES, DEBUGINFO_EXTERNALIZE (CMakeLists.txt:46-70)
  - L"ninja check" -> add_custom_target(check ... ctest) exists only with BUILD_TESTS=ON (CMakeLists.txt:1858); page does not say so
  - L"Clang >= 14 / GCC >= 13" -> unverified; Dockerfile builds with g++-13
- `upgrade.md` | **stale** | two stories in one page (0.40.0 to 1.0.0, alpha.9 to this release); the binary prints this page's URL
  - L"upgrading an existing node from 0.40.0 to 1.0.0" -> 1.0.0 is not released; the actual path is alpha.9/alpha.10 data to this release (changelog.md `[Unreleased]`)
  - L"1.0.0-alpha.9 and the alphas before it" -> alpha.10 data is Xapian too (behavior-changes-audit changelog gap 4); src/xapian_data_guard.h:37 refuses both
  - L"cuuid v6 ... decodes about 120x faster" -> unverified number
  - L"--legacy-ids" -> src/opts.cc:236 true
  - src/xapian_data_guard.h:104 prints https://kronuz.github.io/Xapiand/upgrade/#upgrading-from-100-alpha9: the heading and the file name `upgrade` must stay, or the guard's URL changes (OWNED docs-upgrade-page, which writes `upgrading.md`)
- `options.md` | **missing content** | everything it says checks out; documents 24 of the 53 options in src/opts.cc (counting --legacy-ids, --dump, --restore, --database-pool-size, which other pages mention); 29 are on no page
  - L"Searching many indexes" -> matches src/opts.cc:160-170 (defaults 100000, 0, 8, 10 s)
  - L"--shards ... (5)" -> src/opts.cc:43 true
  - L"-vvvv enables --human --echo --pretty --comments" -> src/server/search_views.cc:2190-2237 true
  - L"--tls-key mode 0600; warns within 14 days" -> src/tls.cc:54,73 true
  - on no page: --bind-address, --max-clients, --max-files, --schema-pool-size, --schema-versions-size, --scripts-cache-size, --resolver-cache-size, --uuid, --admin-commands, --log, --uid, --gid, -P/--pidfile, -L/--logfile, --force, --color, --no-color, --no-pretty, --no-echo, --no-human, --no-comments, --iterm2 and the build-conditional --random-errors-{net,io,db} (src/opts.cc:110-272); the Docker entrypoint's XAPIAND_UID, XAPIAND_GID, XAPIAND_NAME, XAPIAND_DATABASE (contrib/docker/xapiand/entrypoint.sh:33-36) and the XAPIAND_DATA_PATH environment variable (src/main.cc:966) are on no page either
  - barrier switch: OWNED barrier-warning A2 (documented with the other options)
  - not in the sidebar (orphan) (docs/src/lib/sidebar.mjs has no `options`)
- `benchmarks.md` | **stale** | numbers are from the Xapian-era 1.0 alpha against 0.40.0; reproduction commands name wrong paths and a wrong shard default
  - L16-25 table "Xapiand ... ~470 MB, 9.2 MB, ~7,800 docs/s" -> measured before strata (git log: page last edited 5a517b052 only to drop cluster text); no strata run behind them (docs/perf has no matching file: grep "11,500" docs/ finds none)
  - L"pread ... Zstandard-compressed storage" -> describes the Xapian-era storage, not strata files
  - L93 "docs/assets/accounts.ndjson" -> docs/public/assets/accounts.ndjson (harness/loadtest.py:202)
  - L99 "NUM_SHARDS=1 matches Xapiand's default of one shard" -> default is 5 (src/opts.cc:43)
  - OWNED closing-benchmark (its file set lists benchmarks.md)

### Quick-start and tutorials

- `quickstart.md` | **stale** | works for the install and the first requests; says "the twitter index", an index here is `twitter/user` and `twitter/tweet`; no data-directory or port note
  - L46 "the twitter index will be created automatically" -> the indexes are `twitter/user` and `twitter/tweet` (path = index; nested indexes are allowed, changelog.md index-patterns entry)
  - L17 "-p 8880:8880" -> XAPIAND_HTTP_SERVERPORT 8880 (config.h.in:302), correct
- `manipulation.md` | **current** | write/replace/update/scripts examples match the verbs (search_views.cc:546-560); "the user index" should read `twitter/user`
- `exploration.md` | **current** | bank dataset, INFO shape (handler.cc:1999-2061), `_check_at_least` (reserved/query_dsl.h:34), `_values` aggregation; its link `reference-guide/query-dsl` is broken
  - L284 "(reference-guide/query-dsl)" -> page is reference-guide/search/query-dsl
  - L"INFO /bank/ shows ... shards bank/.__1" -> shape matches get_database_info (handler.cc:2061)
- `tutorials/home.md` | **missing content** | index of a single tutorial; text says "section is new" and invites contributions
- `tutorials/spatial-search.md` | **current** | circle plus range query and `_selector` all exist; not in the sidebar (orphan); map screenshot el_cerrito.png is a Google Maps image (OWNED license-closure A5)

### Reference: API overview, documents, indexes, WAL

- `reference-guide/api.md` | **wrong** | DELETE of an index path is 501 not a delete, DELETE / is 405, "alias APIs" do not exist; wide-search and nesting text is right
  - L156-167 "will delete the whole index ... with all its documents" -> DELETE /path/ answers 501 Not Implemented (the page's own e2e block says 501): src/server/search_views.cc:1479-1491 (wrong)
  - L203-204 "`DELETE /` ... names none answers `400`" -> DELETE / answers 405 (delete_database_view checks has_pth first): src/server/search_views.cc:1485-1488 (wrong)
  - L97-99 "Method Override won't work with any other method other than POST" -> a GET (or the same method) is accepted too: src/server/search_views.cc:432-444 (wrong, minor)
  - L314-315 "Most APIs that refer to an index parameter support execution across multiple indexes" -> only SEARCH and COUNT take a list or patterns; GET/HEAD/INFO/PUT/DELETE/DUMP/RESTORE/CHECK/WAL answer 400 "Method can only be used with single indexes": src/server/search_views.cc:839,1087,1231,1290,1393,1575,1610,1759,1794 (stale, Elasticsearch wording)
  - L738 "single-index alias APIs" -> no alias API or word in src/ (grep "alias" hits only opts.cc, y2j.cc): stale Elasticsearch wording
  - L350 "A `*` used to match a `/` as well (`test*` matched `test1/old`)" -> history narrative, not present-tense; the rule is already given above (L322-324) (stale)
- `reference-guide/documents.md` | **missing content** | index lists six CRUD pages and info but omits the Scripting and Versioning pages that sit beside them
  - L5-17 list -> documents/scripting.md and documents/versioning.md exist and are not linked here (missing content)
- `reference-guide/documents/delete-api.md` | **wrong** | the version precondition it documents is ignored by DELETE; trailing-slash warning promises an index delete that answers 501
  - L32-37 "only be performed if the last current document version matches the passed `version` ... 409" -> delete_document_view reads ?version but never passes it; the engine delete takes no expected version (`version_` unused): src/server/search_views.cc:833-851, src/database/shard.cc:1448-1462 (wrong)
  - L17-19 "the later will delete the whole index `/twitter/tweet/1/`" -> 501 Not Implemented: src/server/search_views.cc:1479-1491 (wrong)
  - L13 "The result of the above get operation" -> it is a delete (typo) (stale)
- `reference-guide/documents/exists-api.md` | **current** | HEAD on a document is 200 or 404 through document_exists_view; nothing else claimed
  - none
- `reference-guide/documents/get-api.md` | **stale** | the shown body carries #docid and #shard, which appear only with ?comments or --comments
  - L34-44 `"#docid": 1, "#shard": 1` in the default GET body -> only added when request.comments is set (default off; on at verbosity 4+): src/server/search_views.cc:1882-1889, src/opts.cc:252-255 (stale)
  - L57-58 "latest committed document from the primary shard" -> ?volatile opens the index writable so the read sees the writer's newest view; "primary shard" is cluster-era wording: src/server/search_views.cc:2323-2331, 1834-1838 (stale, minor)
- `reference-guide/documents/index-api.md` | **wrong** | shows an echoed body as the default result, an integer auto id, and an index-templates feature; none match the code
  - L19-31 "The result of the above index operation is: {...}" -> PUT with an id answers 204 No Content with no body unless ?echo / --echo (echo off by default); a body-less POST answers 201 with Location: src/server/search_views.cc:941-985, src/opts.cc:256-259 (stale; docs/src/content/docs/options.md:15-23 says this, this page never mentions it)
  - L28-29 `"#docid": 1, "#shard": 1` -> need ?comments too: src/server/search_views.cc:959-966 (stale)
  - L74 `"_id": 2` for POST without an id -> an id minted by the server is a UUID (v6 codec; v1 with --legacy-ids), not a counter: src/database/schema.cc:2713-2717, 2787, 2804, 2853-2870; src/opts.cc:235 (wrong)
  - L43 "applies any index templates that are configured" -> there is no index-template feature in src/ (the only `template` hits are C++): wrong, Elasticsearch wording (wrong)
  - L95-100 "Delete operations can be made optional ..." under the Index API -> copy-paste; means index operations. PUT ?version=N (or `_version` in the body) is checked, mismatch is 409: src/database/handler.cc:339-342, src/server/http_utils.h:68-70 (stale)
- `reference-guide/documents/info-api.md` | **missing content** | field list lacks version, shard, endpoint, raw_data; terms are rightly absent
  - L26-31 list (docid, data, values) -> response also has `version`, and `shard` plus `endpoint` when the index has more than one shard, and `raw_data` with ?raw; sizes humanized with ?human: src/database/handler.cc:1915-1975, src/server/search_views.cc:1243-1250 (missing content)
  - no mention of terms (removed on purpose): current (src/database/handler.cc:1915-1975 has no terms)
- `reference-guide/documents/scripting.md` | **wrong** | documents `_method` as the HTTP method (always empty) and a query-scoring use that does not exist; limits table matches code
  - L7 "or evaluate a custom score for a query" -> scripts run only on document write; no script hook in the query DSL, and `scripted_metric` is "reserved but not implemented yet": src/query_dsl.cc (no script), src/aggregations/aggregations.cc:230-243 (wrong)
  - L90 "`_method` HTTP method that triggered the script" -> call_script passes an empty string (`std::string method; // TODO`): src/database/handler.cc:300,323 (wrong)
  - L71-73 "scripts do not have a time-based expiration" -> a foreign script is cached apart and re-read when its document changed, checked after one second: docs/mechanisms/lua-bounds.md item 7, src/lua/processor.h (foreign_revalidation) (stale, minor)
  - L145-173 bounds table: 10^8 instructions, 5x10^8 work, 16x10^6 per call, 64 MiB, 200,000 stack, 16 MiB / 262,144 units -> match src/lua/processor.h:50-62 and lua-bounds.md (current)
- `reference-guide/documents/store-api.md` | **missing content** | page is right for small blobs; says nothing of the 16 MiB streamed upload, the 4 GiB cap or the 1 MiB cap on non-stored bodies
  - none stale found
- `reference-guide/documents/update-api.md` | **wrong** | upsert is a verb not a query param, first sentence says "delete", a sample _version is off, PATCH is undocumented
  - L5 "allows to delete a document based on the existent content" -> updates (typo changes the meaning) (wrong)
  - L11-12 "passing `upsert` as a query param" -> the UPSERT HTTP method (or `/doc:upsert` mapping) sets create=true; no query param named upsert is read: src/server/search_views.cc:575-579, 1027-1030, 218; grep next("upsert") has no hit (wrong)
  - L83 `"_version": 12` after one PUT and one UPDATE -> 2 (stale)
  - L42-46, L82-86 `#docid`/`#shard` and the bodies -> only with ?echo / ?comments: src/server/search_views.cc:1038-1065 (stale)
  - L181-184 "Delete operations can be made optional ..." under the Update API -> copy-paste; ?version on UPDATE is checked, 409 on mismatch: src/database/handler.cc:339-342 (stale)
- `reference-guide/documents/versioning.md` | **wrong** | says an older version arriving late "can simply be ignored"; the code is compare-and-set on a caller-supplied version, else the last write wins
  - L10-14 "an older version of a document never overwrites a newer version ... If an older version arrives after a new version, it can simply be ignored" -> a write without ?version always applies and bumps _version; with ?version=N (or `_version` in the body) a mismatch answers 409 Conflict and nothing is ignored silently: src/database/handler.cc:339-342; src/engine/strata/shard.cc:604-611, 626-629; src/server/http_utils.h:68-70 (wrong)
  - L29-35, L98-104, L135-141 shown PUT/GET bodies -> a PUT answers 204 unless ?echo; GET shows no #docid unless ?comments (stale)
  - L107-110 `version=1` query or `_version` in the body -> both read: src/server/search_views.cc:2364-2367; src/database/schema.cc:2933 (current)
- `reference-guide/indexes.md` | **missing content** | list omits the Backups page; no link to COMMIT/CHECK/WAL index-level verbs
  - L5-18 -> indexes/backups.md is a sibling page and is not linked (missing content)
- `reference-guide/indexes/backups.md` | **wrong** | claims NDJSON dumps (Accept ndjson answers 406); omits RESTORE limits and response; CLI part is right
  - L72-76 "`Accept` header to either `application/x-msgpack` or `application/x-ndjson`, you can dump data to any of those formats" -> DUMP content negotiation offers JSON, YAML and MsgPack only; Accept ndjson answers 406 "Response type not accepted"; NDJSON is accepted only as RESTORE input (changelog L1023): src/database/data.h:99-102, src/server/search_views.cc:1620-1625, 2520-2537 (wrong)
  - L86, L95, L113, L141 `/twitter/tweets/` vs dumped `/twitter/tweet/` -> inconsistent index name in one flow (stale, minor)
  - L20, L28 `xapiand --dump="twitter/tweet" --out=...` / `--restore ... --in=...` -> exist: src/opts.cc:118-124, src/main.cc:840-895 (current)
  - L31-34 "exact same parameters you use while running the server" -> still needed (the dump opens the data directory): src/main.cc:840-895 (current)
- `reference-guide/indexes/close-api.md` | **current** | OPEN and CLOSE are answered 501 inline; page says unimplemented and the e2e blocks pin 501
  - none
- `reference-guide/indexes/create-api.md` | **stale** | the "Index Name Limitations" list (243 bytes, forbidden characters, no dots) is not what the code enforces; settings text is right
  - L28-33 forbidden characters, no `.` after the first, "243 bytes" -> src/ has no 243 check; what is refused: NUL, control bytes, backslash, malformed UTF-8 (400), empty path or one outside the data directory (400), `..` is normalised away: src/index_path.h:36-76, src/endpoint.cc:237-249; a name with `*`, `?`, `[` is a pattern in SEARCH/COUNT only (stale)
  - L54-56 default 5 shards; `number_of_replicas` accepted and stored, no effect -> NUM_SHARDS 5, replicas accepted: src/opts.cc:43,152-153; src/index_resolver_lru.cc:574-583 (current)
  - L59-61 changing number_of_shards on an existing index -> 400 "It is not allowed to change 'number_of_shards' setting": src/index_resolver_lru.cc:590-596 (current)
- `reference-guide/indexes/delete-api.md` | **current** | page says unimplemented and the e2e pins 501; the warning text still describes the delete in the future tense, consistent with the banner
  - none
- `reference-guide/indexes/exists-api.md` | **stale** | "index (indexes)" implies HEAD takes a list; it answers 400 for more than one index
  - L5 "check if the index (indexes) exists" -> HEAD /a,b/ answers 400 "Method can only be used with single indexes": src/server/search_views.cc:1288-1291 (stale)
- `reference-guide/indexes/get-api.md` | **stale** | "one or more indexes" is false; GET on a list answers 400
  - L5 "retrieve information about one or more indexes" -> single index only: src/server/search_views.cc:1385-1388 (stale)
  - L13-18 e2e: response has `_schema` and `_settings` -> true: src/server/search_views.cc:1368-1374 (current)
- `reference-guide/indexes/info-api.md` | **stale** | `revision` is described as a document ID; uuid/revision/shards behaviour is right
  - L31 "`revision` - Internal document ID" -> the index's commit revision (stats.revision): src/database/handler.cc:2005 (stale)
  - L24 "thewhole" typo (stale, minor)
  - L30-39 field list -> matches single-shard answer (endpoint, uuid, revision, doc_count, last_id, doc_del, av_length, doc_len_lower, doc_len_upper, has_positions) and multi-shard answer (adds shards, drops uuid and revision): src/database/handler.cc:1996-2064 (current)
- `reference-guide/wal.md` | **stale** | recovery is at each index's next writable open, not at startup; spelling/synonym entries are never produced; the WAL verb and HTTP RESTORE bypass are not mentioned
  - L16-19 "it recovers on startup by replaying the WAL" -> index shards replay lazily when first opened writable; only the registry shards open at startup: src/database/shard.cc:491-502; docs/mechanisms/wal-replay.md "Failure" (stale)
  - L6-7 "spelling and synonym updates" -> no HTTP or indexing path calls add_spelling/remove_spelling; no synonym writes: docs/design/xapiand-on-strata.md:71 (stale)
  - L29-31 "Certain internal paths ... most notably a command-line restore" -> an HTTP RESTORE also opens with DB_DISABLE_WAL: src/server/search_views.cc:1663 (stale, minor)
  - L26-27 `--writers`, `--wal-writer-cache-size` -> exist: src/opts.cc:154-156,179-180 (current)

### Reference: schemas

- `reference-guide/schemas.md` | **stale** | Index page is accurate except one wrong link and a legacy type name
  - L86 "You can read more about namespaces [here](indexing-mode)" -> link must point to the namespaces page (/Xapiand/reference-guide/schemas/namespaces); it points at indexing-mode (wrong)
  - L17 "a simple type like `text`, `string`, `keyword`..." -> `string` is a legacy alias of `text`, `term` of `keyword`: src/database/schema.cc:942-951, src/reserved/types.h:32,35 ("FIXME: remove legacy") (stale)
- `reference-guide/schemas/meta-data.md` | **current** | `_meta` and `._schema._meta` selector both match the code
  - (checked: src/reserved/schema.h:28 RESERVED_META; src/server/search_views.cc:1371 selector applies to {_settings,_schema})
- `reference-guide/schemas/casting-types.md` | **current** | `{"_integer": 2221.82}` truncates to 2221 (src/cast.cc:163); compat table lives in casting-values
- `reference-guide/schemas/namespaces.md` | **wrong** | Text says a shallower path matches; the page's own example expects count 0; depth limit undocumented
  - L61-62 "every level of the path is also indexed on its own, so a shallower, partial path matches as well" -> partial paths skip intermediate levels (`style.footwear` reaches `style.clothing.footwear`); a truncated path (`style.clothing` -> "khakis") does NOT match, as the e2e at L83-86 (count 0) itself says: src/database/schema.cc:4516-4560 get_partial_paths; docs/.../namespace-queries.md "Partial Paths" (wrong)
  - L96-98 "By default namespaced fields keep information for each level" -> default is ON for a namespace unless `_partial_paths:false`: src/database/schema.cc:7455-7456, 7831-7832 (true)
  - L129 "Partial paths can be used for Namespace Queries" -> fine, but no limit stated; see MISSING partial-paths depth
- `reference-guide/schemas/field-types.md` | **stale** | Two broken anchors, UUID filed under Geospatial, aliases and `date` type absent
  - L25 "../field-types/date-type/#time-delta" and L24 "#time" -> date-type headings are "Time Datatype" / "Time Delta Datatype", slugs `time-datatype` / `time-delta-datatype`: date-type.md:116,133 (wrong link)
  - L33-35 "## Geospatial datatypes ... ### UUID datatype" -> UUID is a concrete type, one heading level too deep under Geospatial (stale layout)
  - L8-41 list omits `date` (own type, separate default accuracy) and the aliases `float`/`floating`, `geospatial`/`geo`, `term`, `string`: src/database/schema.cc:905-960, src/database/schema.h:205-225 (FieldType: date, script, geo, ...) (stale)
  - L41/L47-52 `Script`, `Object`, `Array`, `Foreign` listed with no page: `foreign/array/<type>` composites are accepted: src/database/schema.cc:880-897 (missing link only)
- `reference-guide/schemas/accuracy.md` | **missing content** | Describes numeric/date; geo is promised but absent, and terms-only fields are not exact
  - L5 "Numeric, date and geospatial fields" but no geospatial section -> HTM levels 0..25 (`HTM_MAX_LEVEL`, htm repo htm.h:39), default [3,5,8,10,12,15]: src/database/schema.cc:192-203, 4684-4694 (missing)
  - L41-42 "the results are exactly the same as without accuracies, only faster" -> true only when the field has a value slot; on a terms-only field (`_index: field_terms`) the closed range answer is the union of buckets (wider by < one finest bucket) and open-ended or too-wide ranges answer 400: docs/mechanisms/numeric-range-terms.md "Terms are a prefilter", "Terms-only fields"; src/query_dsl.cc:453 (missing)
  - L11 "`_accuracy` option ... sensible default" -> accuracies only apply when the field has terms (`index & TERMS`): src/database/schema.cc:4707; `_accuracy_prefix` exists, undocumented: src/reserved/schema.h:37 (missing)
  - L74-75 default numeric [100,...,100000000] -> true: src/database/schema.cc:151-158
  - L79-81 date levels second..millennium -> true: src/database/schema.cc:208-223; defaults differ per type (date: day..century; datetime: hour..century; time/timedelta: minute,hour) and only datetime is documented (date-type.md:58): src/database/schema.cc:161-188
- `reference-guide/schemas/dynamic-field-names.md` | **missing content** | Folds "dynamic names" into `_namespace`; the UUID-as-field-name case that uuid-type.md links here for is absent
  - L10 "Dynamic field names solve this by enabling `_namespace`" -> code also treats UUID-looking keys as dynamic names (`<uuid_field>`, `_index_uuid_field`): src/reserved/fields.h:32, src/database/schema.cc:5896-5900 detect_dynamic; uuid-type.md:25-26 points readers here (missing)
- `reference-guide/schemas/foreign-schemas.md` | **stale** | Right mechanism and location; ID-type remark contradicts the page's own example, one typo
  - L31-33 "the ID ... in the `headlines` index is of type `"string"`" -> the example sets `_id` to `keyword`, and `string` is a legacy alias of `text`, which an id can never be (it is switched to keyword): src/database/schema.cc:9120-9126 (stale)
  - L35 "index `hedlines`" -> typo for `headlines` (wrong)
  - L5-8 "stored inside the corresponding index in `.xapiand/indexes`" -> true; foreign link is `.xapiand/indexes/<path with / as %2F>`: src/database/schemas_lru.cc:313 (true, no row needed)
- `reference-guide/schemas/dynamic-typing.md` | **wrong** | Numeric detection is on by default and never reads strings; detected-type list is incomplete; two examples are wrong
  - L53-54 "a field called `count` with datatype `integer`" -> the example field is `age` (wrong)
  - L116-117 "numeric detection (which is disabled by default)" -> `numeric_detection` defaults to true: src/database/schema.cc:1899 (wrong)
  - L129-138 `"my_float": "1.0"` detected as `float`, `"1"` as `integer` -> guess_concrete_type's STR branch has no numeric branch; numeric_detection governs JSON numbers only (positive/integer/floating cases); a numeric-looking string becomes text: src/database/schema.cc:5096-5230 (wrong)
  - L101-110 with `datetime_detection:false`, "2015/09/02" "will end up being a `keyword`" -> text_detection is on by default, so it types `text` (a keyword only if `text_detection:false`); also `date_detection` is separate: a plain `YYYY-MM-DD` string still types `date`: src/database/schema.cc:5153-5167, 5203-5209 (wrong)
  - L77-78 "These are the only field datatypes that are dynamically detected" -> also date, time, timedelta, uuid, geospatial (EWKT strings, and a 2- or 3-number array in lon/lat range), boolean from the strings "true"/"false"; order is uuid, date, datetime, time, timedelta, geo, bool, text, keyword: src/database/schema.cc:5144-5215, 5240-5256 (wrong)
  - L75 String row lists datetime, float/integer, UUID, text, keyword -> drop the numeric-string entry, add date/time/timedelta/geo/bool; see above (stale)
  - L82-84 "`datetime_detection` is enabled (the default)" -> true; the other detection flags (`_date_detection`, `_time_detection`, `_timedelta_detection`, `_uuid_detection`, `_geo_detection`, `_bool_detection`, `_text_detection`) all default true and are undocumented: src/database/schema.cc:1895-1903 (missing)
  - L62 "strict mode ... /Xapiand/options#strict" -> true (`--strict`, src/opts.cc:268; options.md:35); the per-schema `_strict` and `_dynamic` properties are not mentioned: src/reserved/schema.h:40-41 (missing)
  - UNVERIFIED reference-guide/schemas/dynamic-typing.md:L46-49 and :L69-72 (see below)
- `reference-guide/schemas/explicit-types.md` | **current** | `_type` first-set rule and type names match; rejection of a negative `positive` is unprobed
- `reference-guide/schemas/indexing-mode.md` | **missing content** | The ten modes and default are right; the aliases and comma forms the code accepts are not listed
  - L19-30 table -> code also accepts the aliases `field` (= field_all) and `global` (= global_all) and any comma pair such as `global_terms,field_values`, `field_terms,global_values`: src/database/schema.cc:561-690 (_get_index) and the accepted list at 518-560 (missing)
  - L32 "default is `field_all`" -> true: src/database/schema.h:121 DEFAULT_INDEX; `_id` is forced to field_all and bool_term: src/database/schema.cc:9107-9120 (true)
  - L8-15 terms/values semantics -> true in code (src/database/schema.cc:5337-5345, 5275); a terms-only field has range limits, see MISSING terms-only ranges
- `reference-guide/schemas/field-types/boolean-type.md` | **missing content** | Accepted strings right; says strings need an explicit `_type`, but "true"/"false" strings detect as boolean by default; `_bool_term` absent
  - L5-7 "but you have to explicitly use the `_type` boolean" -> only for `t`/`f`/`1`/`0`; the strings "true" and "false" are typed boolean by default (`bool_detection` true): src/database/schema.cc:5193-5202, 1901 (stale)
  - L31-32 accepted strings true/t/1, false/f/0 -> true (also case-insensitive T/F/TRUE/FALSE): src/serialise.cc:670-700
  - L36-43 parameters -> `_bool_term` (boolean term, no weight; default false) is accepted by every term type but is not listed: src/database/schema.h:118, src/database/schema.cc:5301 (missing)
- `reference-guide/schemas/field-types/date-type.md` | **stale** | One generated-terms sentence contradicts the default accuracy; type/value-encoding text is owned by another quest
  - L56 "In the above example terms for the day, month and year are generated" -> default datetime accuracy is hour, day, month, year, decade, century (six levels), the page's own L58-62: src/database/schema.cc:170-177 (stale)
  - L15-16 "stored as a floating point number representing seconds-since-the-epoch" -> value representation; see OWNED
  - L108-114 params -> `_accuracy` list incl. second..millennium true: src/database/schema.cc:208-223; the `date` type's default accuracy (day..century) and time/timedelta default (minute, hour) are not stated: src/database/schema.cc:161-188 (missing)
  - L78-87 units y M w d h m s -> true: datetime repo datetime.cc:566-583
  - L96-102 `2019-01-01||-18y+4M+23d` -> 2001-05-24: arithmetic checks (true)
  - Range limit for terms-only datetime fields (open end or too wide -> 400) is not mentioned: docs/mechanisms/numeric-range-terms.md "Dates" (missing)
- `reference-guide/schemas/field-types/keyword-type.md` | **missing content** | Behaviour right; the 245-byte term limit that returns 400 is not documented
  - whole page -> a keyword whose term (field name + value) exceeds 245 bytes answers `ClientError` "Keyword too long" (400); for other types an over-long term is silently not indexed: src/database/schema.cc:5301-5306 (missing)
  - L9 "a field with text values is interpreted as `text`" -> true (text_detection default true, src/database/schema.cc:5203)
  - L25 changing a field's type returns an error -> true (ClientError "Type mismatch", src/database/schema.cc:5206-5214)
- `reference-guide/schemas/field-types/numeric-type.md` | **stale** | Accuracy paragraph describes an old trie/string-encoding picture; the range mechanism is bucket terms plus an exact value check
  - L14-21 "trie indexing numerical values in a special string-encoded format... lexicographic sortable string representations... center of the range searched at the lowest precision in the trie" -> a value is indexed under bucket terms [k*a,(k+1)*a) per accuracy; a range is a flat OR of bucket terms (128-term budget) used as a prefilter, the exact value source decides when the field has values: docs/mechanisms/numeric-range-terms.md "Terms are a prefilter", "Decomposition", "The budget"; src/multivalue/range_terms.h (stale; also OWNED)
  - L23-27 default accuracy [100,...,100000000] -> true: src/database/schema.cc:151-158
  - L8-10 integer/positive/float widths -> true (src/database/schema.cc:5421 positive via u64, floating f64)
  - L34 `_accuracy` array of numbers -> true; must be an array, else 400: src/database/schema.cc:8057-8067
- `reference-guide/schemas/field-types/text-type.md` | **missing content** | Stemmer/stop strategy tables and defaults are right; positions, spelling, n-gram and position parameters are not listed
  - L84-95 parameters -> also accepted: `_positions`, `_spelling`, `_ngram`, `_cjk_ngram`, `_position`: src/reserved/schema.h:30-35,32; src/database/schema.cc:6418-6460 (dispatch list); defaults positions true, spelling false: src/database/schema.h:117-118 (missing)
  - L59-60 `_stem_strategy` default stem_some, L71 `_stop_strategy` default stop_stemmed -> true: src/database/schema.h:114-115; names/aliases at src/database/schema.cc:431-485 (true)
  - L87 "`_language` default is `none`" -> no default language is set in the code (empty); stemming off unless `_language`/`_stem_language` given, as the Caution at L51-55 says (true in effect)
  - CJK: `_cjk_ngram` stays, `_cjk_words` is gone (changelog [Unreleased] Removed); the page correctly says nothing about it, but a reader of the changelog finds no CJK note here (missing, optional)
  - stems keep the `Z` prefix as the field `Z<name>`: docs/mechanisms/seam-fields.md "How it works" 4 (true)
- `reference-guide/schemas/field-types/geospatial-type.md` | **wrong** | Wrong type name, an unparseable distance example, a "formats that work" list that contradicts its own unimplemented hint, mislabelled table, typo, tips that invert coordinate order
  - L5 "Fields of type geo_point" -> the type is `geospatial` (alias `geo`); there is no `geo_point`: src/database/schema.cc:928-934 (wrong)
  - L210 `"2.72km"` as a distance -> distance strings are parsed with `strict_stoi`, integer only (`2km`, `272m`); "2.72km" throws GeoSpatialError "Object must be a valid metric or imperial distance string"; a JSON float number of meters is fine: src/geospatial/geospatial.cc:84-88 (strict-stox repo README: strict_stoi returns int) (wrong)
  - L166-172 "Here are examples of formats that work: DMS, DMM, DD text" -> contradicts L186-189 (unimplemented hint); a text value goes to the EWKT parser only, so `41.50343, -74.01042` and DMS/DMM strings answer `GeoSpatialError`: src/geospatial/geospatial.cc:105-109, src/geospatial/ewkt.cc:766-809 (wrong)
  - L182 "List your latitude coordinates before longitude coordinates" -> array form and EWKT are longitude first (L157-159, 202-204 say so; src/database/schema.cc:5243 "GeoJSON requires longitude first") (wrong)
  - L137 "MULTIPCIRCLE" -> typo; the EWKT name is `MULTICIRCLE`: src/geospatial/ewkt.cc:45 (wrong)
  - L131-140 EWKT list -> also GEOMETRYCOLLECTION and GEOMETRYINTERSECTION; `SRID=<n>;` prefix: src/geospatial/ewkt.cc:49-50, 86 (missing)
  - L118-124 table headed Latitude / Longitude / Height -> the rows are unit-sphere Cartesian x,y,z (each row's squares sum to 1.0), not latitude/longitude (wrong)
  - L18-114 JSON shapes -> `_point`, `_circle`, `_polygon`, `_chull`, `_ewkt` right; also accepted: `_convex`, `_multipoint`, `_multicircle`, `_multiconvex`, `_multipolygon`, `_multichull`, `_geometrycollection`, `_geometryintersection`, and the members `_lat`, `_lon`, `_altitude`/`_alt`, `_units` ("degrees"/"radians"), `_srid`: src/reserved/types.h:41-53, src/reserved/geo.h:28-36, src/geospatial/geospatial.cc:112-153, 239-280 (missing)
  - L212-229 units mm cm m km in ft yd mi nmi NM -> true: src/geospatial/geospatial.cc:52-80
  - L242-249 default accuracy [3,5,8,10,12,15] -> true: src/database/schema.cc:195-202; valid levels are 0..25 only: src/database/schema.cc:4684-4694 (missing)
  - L233-240 Accuracy paragraph (trie / string-encoded) duplicates numeric-type.md; see OWNED
  - geospatial params `_partials` (default true) and `_error` (default 0.3) are not documented: src/database/schema.h:116-117, src/database/schema.cc:8107 (missing)
- `reference-guide/schemas/field-types/geospatial-type/htm.md` | **stale** | Level table runs to 30 and the text to 31; the schema accepts levels 0 to 25 only
  - L14-15 "runs out of bits at level 31" and L34 level 30 row -> HTM_MAX_LEVEL is 25 (~0.3 m), an `_accuracy` level above it answers 400: htm repo htm.h:39; src/database/schema.cc:4684-4694 (stale)
  - L17-20 distance formula and L22-34 rows -> arithmetic checks (level 0 = 10,007,557.2 m; level 25 = 0.298 m) (true)
  - L12 image /Xapiand/assets/htm.png and L37 link /Xapiand/tutorials/spatial-search/#searching -> both exist (docs/public/assets/htm.png; tutorials/spatial-search.md:77) (true)
- `reference-guide/schemas/field-types/uuid-type.md` | **stale** | Generated ids are UUIDv6 by default; page describes v1-only condensed ids and versions 1/3/4/5
  - L5 "version 1, 3, 4, and 5 UUIDs" -> validity is a format check, not a version check, and the default id codec is v6: src/database/schema.cc:2713-2717, src/serialise.cc:1185, src/opts.cc:236 (stale)
  - L17-21 "a subset of UUID 1 ... condensed" -> v1 condensed `~...` ids still decode; ids the server mints are v6 with an 8-byte compact form unless `--legacy-ids`: docs changelog.md [1.0.0-alpha.5], upgrade.md:44; src/opts.cc:235-238 (stale)
  - L25-26 UUID field names -> Dynamic Field Names -> true: src/database/schema.cc:5896; `_index_uuid_field` values uuid / uuid_field / both, default uuid: src/database/schema.h:122, src/database/schema.cc:445-470 (true)
  - L26 link ../../dynamic-field-names -> lands on a page that never mentions UUID names (see dynamic-field-names.md)

### Reference: search API and aggregations

- `reference-guide/search.md` | **current** | Index page links to Search API, Query DSL, Aggregations; all targets exist; only a typo ("an complementary")
  - L9 "an complementary information" -> grammar, "complementary information" (typo)
- `reference-guide/search/search-api.md` | **missing content** | Both examples true (SEARCH, collapse); page documents none of the parameters, response, limits the code has
  - L61 "`collapse_max` ... (it is 1 by default)" -> true: utils.h:90; collapse_max=0 turns collapse off: handler.cc:1218, strata/search.cc:696 (not stated, minor)
  - (all other claims verified: collapse semantics strata/search.cc:695-763 and docs/mechanisms/strata-search.md:55-59; "total counts the hits that are left" handler/search_views.cc:2003; default limit 10 search_views.cc:2447)
- `reference-guide/search/aggregations.md` | **wrong** | Says nesting has no hard limit (16 levels, 65,535 buckets answer 400); "on every node"; "four families" lists two
  - L40 "There is no hard limit on the level/depth of nested aggregations" -> 17th nested level answers 400 "nested deeper than the limit of 16 levels": aggregations.h:40 AGGS_MAX_DEPTH=16, aggregations.cc:check_depth; also 65,535 live buckets per search -> 400: aggregations.h:39, bucket.h aggregate() throw; changelog.md:195-199 (wrong)
  - L70 "cover every matching document, on every node" -> one node only now; still true that check_at_least is forced to CHECK_ALL for any search with aggregations: handler.cc:1224-1227 (stale wording "on every node")
  - L15-16 "break them into four main families" -> two are listed (Metric, Bucketing); Elasticsearch's pipeline and matrix families do not exist here: aggregations.cc dispatch (wrong)
  - L75 "`_aggregations` ... (the key `_aggs` can also be used)" -> true: aggregations.cc:116-119, reserved/aggregations.h:28-29. OK.
  - L47 "operate on the `double` representation" -> accumulators are `long double` (metrics.h sum/avg/count), integer histogram buckets use int64/uint64 (bucket.h HistogramAggregation), results are cast to double (metrics.h get_result) (stale detail, claim of >2^53 approximation still roughly true for results)
  - L92-95 "_meta" unimplemented hint -> true, no `_meta` handling: aggregations.cc:122-228 (an unknown key where the type is expected answers 400 "Aggregation type ... is not valid", aggregations.cc:238)
  - L118-128 "_value_type" under the "Unimplemented" hint -> true, no `_value_type` in src/ (the paragraph reads as if it works; hint is correct)
  - L73-81 aggregation names -> a name that is empty or starts with "_" or "#" answers 400 "Aggregation name ... is not valid": aggregations.cc:121-123 + reserved/reserved.h:37 (MISSING from the page)
  - L5-12 / L51-68 structure and top-level `_limit: 0` -> true: handler.cc:1188-1195.
- `reference-guide/search/aggregations/bucket.md` | **wrong** | "Not hard limited" is false (65,535 buckets/16 levels); keyed response snippet omits buckets and uses "0.0" keys; sort forms incomplete
  - L296-297 "The maximum number of buckets allowed in a single response is not currently hard limited" -> a search holds at most 65,535 live buckets (all levels) and 16 nesting levels, past either 400: aggregations.h:39-40, bucket.h aggregate() (the `_limit` default 10000 is true: bucket.h _conf_limit) (wrong)
  - L352-387 "Response:" keyed histogram snippet -> keys are "0","1000",... not "0.0","1000.0" (e2e at L344-345 and histogram-aggregation.md:84 expect "0"/"1000"; keys come from strings::format of the bucket, bucket.h HistogramAggregation::aggregate_float); snippet also drops the 8000 (7) and 9000 (1) buckets, so it sums to 992 not 1000 (histogram-aggregation.md:83 has them) (wrong)
  - L300-307 shard_size paragraph -> true: default `_limit + _limit/2 + 10`, never below `_limit`, _prune at 2x, geo -> 400: bucket.h _conf_shard_size, _prune, ValuesAggregation/TermsAggregation ctors. Only `_terms`/`_values` are capped (bucket.h `capped=true`); the structure block at L51 lists `_shard_size` for every bucket type, but histogram/range/filter ignore it (stale generality)
  - L392-432 "_min_doc_count ... match more than a configured number" -> buckets with doc_count >= value are kept ("5 hits or more" at L431 is right, "more than" at L392 is wrong): bucket.h _get_result, _conf_min_doc_count (accepts 0)
  - L62-65 "_sort" -> besides {"_doc_count":"asc"} and {"_key":"asc"}, code also accepts a bare string ("_doc_count", "_key", or a sub-aggregation path => ascending) and {"<path>": {"_order":"asc|desc"}} forms; any other order 400: bucket.h _conf_sort (MISSING)
  - L241-286 Deep Ordering "unimplemented" with e2e status 400 -> true: path "cities.*.balance_stats._max" fails in bucket.h aggregate() "Field '*' not found" (AggregationError = ClientError, exception.h:25), 400
  - L436-466 include/exclude, collect mode, `_missing` unimplemented hints -> true, nothing in src/aggregations. The `_missing` example (L472-509) is run by the E2E and passes only because the parameter is ignored (gender has two values); it documents behaviour that does not exist
  - L609-617 "Mixing field types" warning -> see UNVERIFIED
  - L27-35 list of unimplemented bucket types -> true, all answer "reserved but not implemented yet" 400: aggregations.cc:222-237
- `reference-guide/search/aggregations/metrics.md` | **wrong** | Says values "can also be generated using scripts"; no script support, scripted_metric answers 400
  - L7-8 "can also be generated using scripts" -> aggregations read only `_field`; no `_script` anywhere in src/aggregations; `_scripted_metric` is "reserved but not implemented yet": metrics.cc ValuesHandler, aggregations.cc:222-237 (wrong)
  - L47-57 structure -> true. Not stated: min/max/sum/avg/variance/std_deviation/stats/extended_stats/median/mode accept only positive, integer, floating, date, datetime, time, timedelta (other types answer 400 "<type> type is not supported"): metrics.h HandledSubAggregation defaults, overrides per class; count accepts every type (MISSING)
  - L23-37 unimplemented list (cardinality, geo bounds/centroid, percentiles, percentiles rank, scripted) -> true
- `reference-guide/search/aggregations/bucket/filter-aggregation.md` | **missing content** | Syntax and example true; multi-key and array semantics not stated
  - L14-19 `_filter`/`_term` -> true: bucket.cc:35-53 (a `_term` object is required, else 400)
  - not stated: several keys under `_term` are ORed (check_single returns on the first filter that matches, bucket.cc check_single), an array value matches any of the values (check_multiple); the match is on the field's stored value slot, not on index terms (MISSING)
- `reference-guide/search/aggregations/bucket/values-aggregation.md` | **stale** | Copy-paste "range aggregations" intro; sample response shown unsorted although default is count descending
  - L10 "structure of range aggregations" -> should be values aggregations (typo, copy of range page)
  - L76-89 Response snippet lists strawberry 76, banana 64, apple 89 -> default order is _doc_count descending (bucket.h:ValuesAggregation default Sort::by_count_desc) and the E2E at L60-61 expects apple 89 first (stale/wrong example)
  - L96-98 default order count descending, `_sort` same as bucket page -> true
  - not stated: `_values` over a geo field answers 400 (bucket.h ValuesAggregation ctor); `_values` accepts every field type and counts the field's stored value slot, so a `keyword` bucket keeps its stored case (docs/status/strata-engine-callbacks.md:36-48)
- `reference-guide/search/aggregations/bucket/terms-aggregation.md` | **stale** | "Prefer _values, more efficient" no longer holds (both read the same stored values); intro says "range aggregations"
  - L12-15 "prefer Values Aggregation ... it's more efficient" -> TermsHandler::values and ValuesHandler::values read the same slot via StringList, no index-term path remains: metrics.cc:ValuesHandler::values / TermsHandler::values; the real differences are that `_terms` takes the field's type from the data field and refuses `text` and fields without values (metrics.cc TermsHandler ctor, 400) (stale)
  - L19 "structure of range aggregations" -> typo, should be terms
  - L8-10 text field answers 400, stored values, once per document and distinct value -> true: metrics.cc TermsHandler ctor, changelog.md:169-171
  - L70-75 expected keys are strings ("38","30") and descending counts -> consistent with bucket.h TermsAggregation (strings::format of the value)
  - not stated: keyword buckets keep their case ("Red" and "red" are two buckets) and numeric fields return their values, not accuracy terms (behavior-changes-audit.md row 47, changelog.md:169-172); geo field -> 400 (bucket.h) (MISSING)
- `reference-guide/search/aggregations/bucket/histogram-aggregation.md` | **missing content** | Core claims true; validation claims not enforced; date, time and timedelta fields and key format not documented
  - L29 `"_interval": "<interval>"` (quoted) -> a string interval answers 400 "'_interval' must be a number": bucket.h HistogramAggregation::configure_* (snippet wrong, should be unquoted)
  - L45-47 "`_interval` must be a positive decimal, `_shift` ... in [0, _interval)" -> only a zero interval is refused ("must be a non-zero number", bucket.h get_bucket); negative interval and out-of-range shift are accepted, not checked (stale/overstated)
  - L9 "interval `500`" vs example at L65 -> example uses 1000 (internal mismatch)
  - L5-6 numeric values only -> also date, datetime, time, timedelta (interval in seconds, via configure_f64); keyword/text/boolean/geo answer 400 "Histogram aggregation can work only on numeric fields": bucket.h HistogramAggregation ctor (MISSING)
  - L18 rounding formula -> true: bucket.h get_bucket(long double)
  - L83-84, L195 expected counts/keys -> consistent with code; keys are plain "0","15"
- `reference-guide/search/aggregations/bucket/range-aggregation.md` | **missing content** | Syntax true; interval semantics, key rules and open-bound behavior not stated
  - L20-21 `_key`, `_from`, `_to` -> `_key` must be a string else 400; default key is "<from>..<to>", "..<to>", "<from>.." or ".." : bucket.h RangeAggregation::_as_bucket and configure_*
  - not stated: a range is half open, `_from` <= value < `_to` (bucket.h aggregate_*); ranges may overlap and a value counts in each; only numeric/date/time field types, others 400 (bucket.h ctor); `_ranges` is required (400 otherwise)
  - L182-185 default order "same order the ranges were listed" -> true: Sort::by_index, bucket.h
  - L36-40 keyed -> true: bucket.h _get_result keyed branch
- `reference-guide/search/aggregations/metrics/count-aggregation.md` | **current** | Counts values of any field type; result key _count
  - L32-34 prose "number of cities" -> `_count` counts values, one per document value, not distinct cities (cardinality is unimplemented); see UNVERIFIED
- `reference-guide/search/aggregations/metrics/min-aggregation.md` | **current** | _min result, numeric types; verified against MetricMin
- `reference-guide/search/aggregations/metrics/max-aggregation.md` | **current** | _max result, numeric types; verified against MetricMax
- `reference-guide/search/aggregations/metrics/sum-aggregation.md` | **current** | _sum result; verified against MetricSum
- `reference-guide/search/aggregations/metrics/avg-aggregation.md` | **stale** | Copy-paste leftovers: "average grade", "avg_grade"; claims otherwise true
  - L66-67 "computes the average grade ... (`avg_grade` above)" -> the example is `avg_age`/`_avg` (leftover text from an Elasticsearch page)
  - L5 sentence starts lower-case "arithmetic mean, also called..." (typo)
  - behavior: `_avg` of zero values is 0: metrics.h MetricAvg::update
- `reference-guide/search/aggregations/metrics/variance-aggregation.md` | **missing content** | Does not say it is the sample variance (n-1); stray number line
  - L55 bare line "2267334.7731415" -> stray text before the JSON block
  - L5 "variance" -> sample variance, (sum_sq - n*avg^2)/(n-1): metrics.h MetricVariance::update; Elasticsearch's is population variance (MISSING). One value divides by zero; see UNVERIFIED
- `reference-guide/search/aggregations/metrics/std_deviation-aggregation.md` | **current** | _std_deviation, _std_deviation_bounds, _sigma non-negative double all match MetricStdDeviation
  - L88-92 "_sigma ... any non-negative double ... 0 is valid" -> true: metrics.h MetricStdDeviation ctor
  - note: it is the sample standard deviation (sqrt of the n-1 variance), not stated
- `reference-guide/search/aggregations/metrics/stats-aggregation.md` | **current** | _count _min _max _avg _sum match MetricStats::get_result
- `reference-guide/search/aggregations/metrics/extended_stats-aggregation.md` | **current** | Adds _sum_of_squares, _variance, _std_deviation, _std_deviation_bounds; matches MetricExtendedStats::get_result
- `reference-guide/search/aggregations/metrics/median-aggregation.md` | **missing content** | No Structuring/Field section; even-count rule and memory cost not stated
  - whole page -> no `{"_median": {"_field": ...}}` structure block or field text like its siblings (MISSING)
  - not stated: with an even count the median is the mean of the two middle values (metrics.h MetricMedian::update); every value of every bucket is held in memory and shipped, so it grows with the document count (no cap)
- `reference-guide/search/aggregations/metrics/mode-aggregation.md` | **missing content** | No Structuring/Field section; tie-break not stated; table typo
  - L14 "Mean (or !verage)" -> typo "Average"
  - whole page -> no structure block (MISSING)
  - not stated: on a tie the smallest value wins (std::max_element over an ordered map, metrics.h MetricMode::update); one counter per distinct value is held in memory

### Reference: query DSL

- `reference-guide/search/query-dsl.md` | **missing content** | Intro is true but states no limits, and calls "match, term" queries what the DSL does not have.
  - L16 "such as the match, term or range queries" -> the DSL has no `match` or `term` clause; leaf clauses are field:value, `_match_all`, `_match_none`, `_in`/`_range`, `_partial`, `_wildcard`, `_fuzzy`, cast words: src/query_dsl.cc:230-280 (stale)
- `reference-guide/search/query-dsl/compound-queries.md` | **missing content** | Lists five true pages; the commented-out block hides `_max`, `_synonym`, `_scale_weight`, `_wildcard` whose real state (modifiers, or 400 as lists) is undocumented.
  - L15-23 (HTML comment) "`_scale_weight`, `_wildcard`" as compound clauses -> given an array they answer 400 "not a valid query": src/engine/query.cc:101-111, changelog.md:238-240; `_max` and `_synonym` work as string modifiers: src/query_dsl.cc:335-340 (stale)
- `reference-guide/search/query-dsl/leaf-queries.md` | **current** | Index of the four leaf pages plus text-queries; all links exist.
- `reference-guide/search/query-dsl/compound-queries/filter-operator.md` | **current** | `_filter` weight from the left only (src/engine/strata/search.cc:236 FILTER = AND with right scaled by 0) and `_and_not` match the code.
- `reference-guide/search/query-dsl/compound-queries/geospatial-searches.md` | **current** | `_point` exact, `_in` plus `_circle` with `_latitude/_longitude/_radius`; all link anchors exist in geospatial-type.md; src/reserved/geo.h:28-34.
- `reference-guide/search/query-dsl/compound-queries/logical-operators.md` | **missing content** | Operators and sum-weights are right; the `_not` weight line repeats its description, `_xor` weight is absent, `_not` over 2+ subqueries is doubtful.
  - L26 "`_not` - Finds documents which don't match any of the subqueries." (under "weight") -> a weight line is expected; `_not` is `match_all AND_NOT x` so it weighs 0: src/query_dsl.cc:287-294, src/engine/strata/search.cc:67 (match-all weighs nothing); prospect docs/status/xapian-ops.md:31 (stale)
  - L18-19 "`_xor` ... A or other or subquery B, but not both" -> garbled; weight is the sum of the matching children: prospect docs/mechanisms/query-operators.md (XOR row) (stale)
  - L15 "`_not` - Finds documents which don't match any of the subqueries" -> see UNVERIFIED below: for 2+ subqueries the code builds `match_all AND_NOT (A AND_NOT B)`, which is not "none of them": src/query_dsl.cc:287-294 with the array branch :413-418 (stale or wrong, needs a probe)
- `reference-guide/search/query-dsl/compound-queries/maybe-operator.md` | **current** | `_and_maybe` required-first, summed weight: src/query_dsl.cc:301, src/engine/strata/search.cc:231 required_optional_scorer.
- `reference-guide/search/query-dsl/compound-queries/range-searches.md` | **missing content** | `_range`/`_from`/`_to` inclusive ends are true; what the code does with open ends, types and string ranges is not said.
  - none stale. Inclusive ends: src/multivalue/range.cc:412-428 (insideRange `>= start`, `<= end`); no bound at all matches everything: :362-364.
- `reference-guide/search/query-dsl/leaf-queries/casting-values.md` | **wrong** | Table lists cast words the query DSL rejects (`_date`, `_time`, `_geospatial`); prose quotes a different number than the example.
  - L98 "`_date` `_time` `_geospatial` `_uuid`" -> the DSL accepts only `_float _positive _integer _boolean _term _keyword _string _text _datetime _uuid _ewkt _point _polygon _circle _chull _multi* _geometrycollection _geometryintersection`; `_date`, `_time`, `_timedelta`, `_convex` are not in the dispatch and answer 400 "Invalid operator"; `_geospatial` is not a word anywhere: src/query_dsl.cc:255-276 and :278-386, src/reserved/types.h:28-53 (wrong)
  - L94-97 rows list `_text` compatible with `_date`, `_time` -> same: use `_datetime` (wrong)
  - L37 "cast `2221.46` to integer" -> the example above it sends `2221.82`; Cast::integer truncates a float: src/cast.cc:156-164 (stale)
- `reference-guide/search/query-dsl/leaf-queries/match-all-query.md` | **current** | `_match_all` weighs 0 (src/engine/strata/search.cc:67), `_match_none` is the empty query (src/query_dsl.cc:342-347).
- `reference-guide/search/query-dsl/leaf-queries/namespace-queries.md` | **current** | `_namespace`, `*`, dotted and partial paths all match; partial paths default on for namespaces: src/database/schema.cc:7455-7456.
- `reference-guide/search/query-dsl/leaf-queries/simple-queries.md` | **wrong** | Says every hit comes with weight, rank and percent; the response carries them only with `?comments`.
  - L56-58 "result is ... together with a weight, a rank and a percent for each" -> `#weight`, `#rank`, `#percent` are written only when `request.comments` is true (default false; `?comments`, `--comments`, or verbosity >= 4): src/server/search_views.cc:2036-2047, :2237-2250, src/opts.h:45 (wrong)
  - L13 "`_limit` ... by default is set to `10`" -> true for the URL `limit` (src/server/search_views.cc:2396-2399); body `_limit` read at src/database/handler.cc:1160-1167.
- `reference-guide/search/query-dsl/leaf-queries/text-queries.md` | **missing content** | Landing page for nine true subpages but says nothing of the queryparser grammar, `_synonym`/`_max`, or that boolean words need `_and`/`_or`.
  - none stale. See MISSING rows "text grammar" and "default-operator modifiers".
- `reference-guide/search/query-dsl/leaf-queries/text-queries/default-operator.md` | **missing content** | `_or`/`_and` are right; the other string modifiers and the effect of choosing one (boolean words on) are not described.
  - none stale. `_and`/`_or` set the BOOLEAN flag: src/query_dsl.cc:817-822. Plain strings do not: the top call passes Op::INVALID, src/query_dsl.cc:1315.
- `reference-guide/search/query-dsl/leaf-queries/text-queries/fuzzy.md` | **wrong** | The default of `_max_expansions` is 50 on the page; the code has no cap for fuzzy words. Parameter table is mislabelled.
  - L5-9 "final query uses up to `_max_expansions` (defaults to 50) matching terms" -> fuzzy max_expansion default is 0 = no limit (DEFAULT_FUZZY_MAX_EXPANSION 0; limit applies only when non-zero; error mode when exceeded): src/query_dsl.cc:73,846; src/engine/strata/expand.cc:121-124. 50 is the cap of `_partial`, not `_fuzzy` (wrong)
  - L87 "`_max_expansions` ... Defaults to `50`." -> same (wrong, though the section is marked unimplemented: parameters are not read, a map under `_fuzzy` is not accepted)
  - L83 "accepted by _Boolean_ fields" -> copy-paste; means the `_fuzzy` query (stale)
  - L73 "`banna~` and `banna~2` are equivalent" -> true, default distance is 2 (src/query_dsl.cc:74, queryparser README `word~` row).
- `reference-guide/search/query-dsl/leaf-queries/text-queries/love-and-hate.md` | **missing content** | `+`/`-` behaviour and "stop words do not apply" are true; the new rejection of a lone exclusion is not said.
  - none stale. "+the" keeps the stop word: queryparser test/vectors/parse-v2.jsonl (row `+the cat` -> `ZSthe AND ZScat`).
- `reference-guide/search/query-dsl/leaf-queries/text-queries/near.md` | **missing content** | `_near` semantics are true; it does not say the operands must be plain words, and a wildcard, fuzzy word or group answers 400.
  - L5 "within 10 words of each other" -> window is 10 + words - 1 (two words: 11): queryparser README (NEAR/n row), test/vectors/parse-ab.jsonl group `default-window` (not wrong, imprecise).
- `reference-guide/search/query-dsl/leaf-queries/text-queries/partial.md` | **wrong** | Says `wildc*` is a partial; `*` is a wildcard and only `**` or `_partial` is partial. Omits the 50-term cap.
  - L13 "`"_partial": "wildc"` or `"wildc*"` would match" -> `wildc*` is a WILDCARD (WILDCARD SYNONYM), `wildc**` and `_partial` are PARTIAL (the last word of two or more characters, most frequent 50 expansions): src/query_dsl.cc:72,756-758,845; queryparser README (`word*`, `ba**`, partial rows) (wrong)
  - L71 "the suffix `**` is equivalent to `_partial`" -> true (src/query_dsl.cc:756-758).
- `reference-guide/search/query-dsl/leaf-queries/text-queries/phrase.md` | **missing content** | Phrase rules (quotes, hyphen, stop words kept) are true; refusal of wildcard, fuzzy or group inside a phrase is not said.
  - none stale. Hyphen, `a:b`, `a/b` read as phrases: queryparser README syntax table.
- `reference-guide/search/query-dsl/leaf-queries/text-queries/simple.md` | **current** | Default operator OR, stemming only with `_language` or `_stem_language` match src/query_dsl.cc:805-848 and src/engine/text.cc.
- `reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md` | **wrong** | Names `few`, `far`, `between` as searched content words; they are in the English stop list. Also a dead link and an unstated `_language` requirement.
  - L39-42 "stop words (`these`, `are`, `and`) are removed ... only (`days`, `few`, `far`, `between`) are searched" -> `few`, `far` and `between` are also in the English list, so only `days` is searched: stopwords/en.txt (grep -x: these, are, and, few, far, between present; days absent); words of a group that are stop words go unless the group is all stop words: queryparser README (wrong)
  - L5-8 "by default any words in the active stop words list will not be searched" -> a field has a stop list only when `_language` is set (default none); an unknown language gives no list and no error: src/query_dsl.cc:830-833, src/stopper.cc:85-92 (stale)
  - L46 "[Love and Hate Modifiers](love-and-hate-modifiers)" -> the page is `love-and-hate`; link is dead (wrong)
  - L47-48 "an empty set of stopwords in the `_stopwords` keyword" -> `_stopwords` is not a reserved word; request in L84-95 answers 400 "Invalid operator" (the page marks it unimplemented and the E2E asserts 400): src/query_dsl.cc:230-280, :388-389 (stale, already flagged)
- `reference-guide/search/query-dsl/leaf-queries/text-queries/wildcard.md` | **missing content** | `*` and the prefix example are true; `?` and in-word patterns need the `_wildcard` modifier and `?` last is literal; no expansion cap is not said.
  - L7 "`?`, which matches any single character" -> pattern flags (`?`, `*` inside) are set only when the glob char is not the last character: a trailing `?` stays literal in a prefix match: src/query_dsl.cc:733-739 (keyword), :775-790 (text) (stale, see UNVERIFIED)
  - L9-10 "a wildcard term should not start with ..." -> advice only: not refused; scans the whole dictionary (empty literal prefix); wildcard max expansion 0 = unlimited: src/query_dsl.cc:71, src/engine/strata/expand.cc:70-100,121 (stale)

### Tests (E2E fixture pages)

- `tests.md` | **missing content** | index lists 15 of 21 fixture pages; carries a stray request that belongs in a tests/schema page
  - L5-19 list "Check Datatypes ... Objects" -> sidebar (docs/src/lib/sidebar.mjs:533-546) also has Fuzzy, Partial, Wildcard, Restore; tests/schema/stem-language.md and basque-language.md are in neither (stale: list not kept up)
  - L21-43 "### Reserved Subfield" with a ```rest PUT /test/reserved_subfield/doc inside the e2e comment -> request is real (runs, expects 400; a `_reserved` key in a field spec throws ClientError "Field ... is not valid", src/database/schema.cc:3040) but sits on the overview page, with an H3 and no body text, so the rendered page shows a bare heading; move it to tests/schema/ (stale shape)
  - L16 "[**_Replace Value by Null_**]" -> the page's own title is "Set Value With Null" (tests/nulltype/replace-null.md:2) (stale, cosmetic)
  - all 15 links hard-code the /Xapiand/ base (astro.config.mjs:49 base '/Xapiand'); fine today, breaks if the base changes -> use relative links (stale-prone)
- `tests/comments/ignore-comments.md` | **stale** | assertion looks at response field `schema`, which a write never returns; passes vacuously
  - L35-38 "for (key in jsonData.schema) ... not.include('#comment')" -> the POST echo (request.echo is added by the generator) returns the document body plus _id/_version, never a `schema`: src/server/search_views.cc:938-958; `for (k in undefined)` does not throw, so the test cannot fail. Fix: GET /test/comment-ignore/ and check `_schema` keys. That '#' fields are skipped is true: src/database/schema.cc:3171, 3797, 4274 (stale shape)
  - L12 `"_id": {"_type": "uuid",}` trailing comma is relaxed JSON; accepted by the repo's JSON reader (harness/README.md: "rapidjson relaxed-JSON") - see UNVERIFIED
- `tests/globalterms/global-terms.md` | **current** | `_index: global_terms` on a uuid field and `SEARCH /test/` hits[0].ident hold
  - (no stale claims. `global_terms` is a valid _index mode: src/database/schema.cc:273,524. `PUT /test/doc` makes index `test` AFTER earlier pages made `test/types`, `test/date`, `test/reserved_subfield`: nested creation in both orders is allowed since 708b6bfe3, src/index_pattern.cc:371ff)
- `tests/nulltype/null-type.md` | **current** | null value on a keyword field round-trips as null
- `tests/nulltype/replace-null.md` | **current** | indexing null into a field first typed integer succeeds and reads back null
  - L2 front-matter title "Set Value With Null" vs H2 "Replace Value by Null" vs sidebar label (cosmetic)
- `tests/schema/create-schema.md` | **current** | PUT with `_schema` then GET index returns `_schema` with the types
  - GET /index/ returns {_settings,_schema}: src/server/search_views.cc:1306-1375 (true)
- `tests/schema/empty-value.md` | **current** | a field spec with no value indexes and the document reads back
- `tests/schema/define-prefix.md` | **current** | _prefix, _index, _ngram, _cjk_ngram schema fields and INFO values keys hold
  - `_cjk_ngram` still exists (src/reserved/schema.h:35); `_cjk_words` is not used (true)
  - L61-75 INFO assertion pins only `values` keys ['0','1']: INFO on a document returns {docid, version, data, values} and no terms (src/database/handler.cc:1918-2000), consistent with the removed term listing
- `tests/schema/define-slot.md` | **current** | _slot 100 appears as a value slot, found by id and by exact range
- `tests/schema/uuid-field.md` | **current** | _index_uuid_field uuid_field/both/default semantics and pinned slot numbers hold
  - slot numbers (296138942, 1407656467, 2761136387, 3856745852) come from get_slot = xxh32(prefix, type): src/database/utils.cc:102; unchanged since v1.0.0-alpha.10 (git diff of that function is empty)
  - `uuid_field` and `both` are the accepted names: src/database/schema.cc:487-507
- `tests/schema/range-search.md` | **current** | accuracy-term over-cover on field_terms (total 3) and exact value-slot ranges match the numeric-range design
  - L120-125 field_terms range 50..150 expects 20,100,180 (3 hits): a terms-only field has no exact check and widens ends to the finest bucket (100): docs/mechanisms/numeric-range-terms.md ("Terms are a prefilter", "Terms-only fields"); default accuracies 100,1e3,1e4,1e5,1e6,1e8: src/database/schema.cc:151-158. True, but the page never says the 3 is an over-cover; a reader reads it as a bug (missing content, see MISSING)
  - `sort=foo` (params) is decorative: field_terms has no value slot to sort on and its 3 hits come back in insertion order 20,100,180; field_values/field_all return 1 hit. The sort is never exercised (weak test)
- `tests/datatypes/check-data-types.md` | **current** | all 11 types index, schema defaults, point search, range search, near miss, out of range hold
  - accuracies: numbers 100..1e8 and date/datetime hour..century and time/timedelta minute,hour: src/database/schema.cc:151-185, 2293-2321 (true; `date` uses def_accuracy_datetime, :2309)
  - L245 INFO pins 13 value slots (xxh32 slot numbers, see uuid-field row) (true)
- `tests/datatypes/date-type.md` | **current** | date formats 'yyyy/mm/dd hh:mm:ss', ISO date, ISO Z, epoch normalise as asserted
  - '/' separators are accepted by the fallback date_re in the datetime library (/Users/kronuz/code/Kronuz/datetime/datetime.cc:37, 89); epoch float -> ISO is not checked in the library; see UNVERIFIED
  - L63-95 the second GET request sits INSIDE the e2e comment (rest_inside_comment) so the rendered page shows heading "Get date type format yyyy-mm-dd" with no request (cosmetic; harness still runs it)
- `tests/namespace/namespace.md` | **stale** | six strict-index examples carry number_of_replicas (no effect on one node, yet required by strict mode); two assertion blocks leak onto the rendered page
  - L371-376, L479-484, L593-598, L627-632, L746-751, L1184-1189 "number_of_replicas": 1 -> accepted and stored, no effect (the cluster is gone; docs/src/content/docs/reference-guide/indexes/create-api.md:55 says so), but src/index_resolver_lru.cc:~573-585 throws MissingTypeError "Value of 'number_of_replicas' is missing" for a strict new index that omits it, so the examples cannot simply drop it. Decision for the owner: keep the example as is and document the requirement, or stop requiring it in strict mode (code change) (stale)
  - L371-376 etc. "number_of_shards": 5 -> equals the default (src/opts.cc:43 NUM_SHARDS 5); max 9999 and not changeable after creation: src/index_resolver_lru.cc:~560-575 (true)
  - L399-415 "Get Strict Namespace" and L497-517 "Get Strict Namespace Array": the ```js assertion blocks are outside <!-- e2e --> (js_outside_comment at L403, L409), so the rendered page prints raw pm.test code (cosmetic, harness runs them)
  - L675 heading "Info Strict Namespace Array" is reused for the namespace_text INFO; L982-1003 INFO assertion carries a dead helper `tobytes` (cosmetic)
  - L476 one request ("Index Strict Namespace Array" PUT) has no assertion block (gets the default 2xx)
  - nested index paths `/test/namespace/...`, `/test/namespace/types/integer/` under existing `test/namespace`: allowed now (src/index_pattern.cc:371ff; docs/src/content/docs/reference-guide/api.md:356). `ade61e145` had renamed such examples; 708b6bfe3 restored them. This page is the live nesting coverage (positive)
  - `_accuracy_prefix` ['<64>.','<3e8>.',...] and geo ['<3>.',...]: prefix format "<hex>." from src/database/utils.cc:112-122 get_prefix(unsigned long long), used by get_acc_prefix schema.cc:1604 (true)
  - L586-612 "Namespace different nested types" expects 400 for mixed keyword/numeric under one `_namespace`: no reason pinned; the 400 would also arise from any other error in the body (weak: assert the message)
- `tests/datastructures/arrays.md` | **current** | array schema types, INFO value slots and AND searches over arrays of text/arrays/objects hold
  - INFO value strings '\u0000\u0001A\u0001B...' are the StringList serialisation (SERIALISED_LIST_MAGIC '\0', then length-prefixed items): src/serialise_list.h:31, 302; a single element is stored bare ('A', L62) (true)
  - slot 1680431078 etc. = xxh32 slots, unchanged (see uuid-field)
- `tests/datastructures/objects.md` | **current** | object, nested-object, complex-object schema types and field searches hold
  - L549-561 `"_language": "en"` accepted: src/database/schema.cc:1644,1730 (true)
  - L716-748 "stop word ... is not indexed": `personality: the` ANDed with `behavior` expects total 0 (the query does not drop stop words when `_language` is set) - see UNVERIFIED
  - no use of `_terms`, `_elite_set`, `?nearest`, `_cjk_words`, INFO term lists
- `tests/restore/restore.md` | **current** | RESTORE of an ndjson file answers 2xx with unique ids per item
  - L5-35 body `@fruits.ndjson` (docs/public/assets/fruits.ndjson: 5 lines, no trailing newline; 188 bytes, far under the 16 MiB cap, src/server/search_views.cc:3182) -> items carry `_id`: src/database/handler.cc:2255-2274 (true). Assertion only checks ids are unique, not that items.length == 5 or that `indexed`/`total` are 5 (response fields prepared/processed/indexed/total/items: search_views.cc:1707-1712). A restore that dropped the first line (an open bug recorded in docs/status/numeric-range.md "CR5") would pass. Weak test, see MISSING
  - L7 `RESTORE /test/restore/` is index `test/restore` under index `test`: allowed (nesting)
- `tests/schema/stem-language.md` | **current** | Italian elision "all'art" indexes and is found by phrase query
  - orphan: not in sidebar.mjs, not in tests.md; harness runs it (after the ranked pages, `basque-language` before `stem-language`). Add both to the sidebar or fold them into one "Languages" page
- `tests/schema/basque-language.md` | **current** | Basque `etxeak` indexes and is found; `basque` is a valid _language (src/database/schema.cc:1634,1690)
  - same orphan finding as stem-language
- `tests/querydsl/fuzzy.md` | **current** | query-string `word~3` fuzzy on keyword and text fields finds the document
  - L6 H2 "Search keywords and text with partial" -> it is the fuzzy page (copy-paste), L11 "Index strin values" typo (cosmetic)
  - `field:unserten~3`: keyword path handles `~N` as edit distance (DEFAULT_FUZZY_EDIT_DISTANCE 2, N override): src/query_dsl.cc:73-74, 711-724, 744-748. This is the query-string `~`, NOT the removed `?fuzzy` URL parameter (no `?fuzzy`/`?nearest` appears in group E)
- `tests/querydsl/partial.md` | **current** | `field:cate**` on keyword and `field:dynamic cate**` on text find the document: src/query_dsl.cc:710-731 (keyword **), :790-796 (text **)
  - L6 H2 "Search keywords and text with partial" is right here; L100 "Index keywords values" fine
- `tests/querydsl/wildcard.md` | **missing content** | the keyword wildcard search asserts only 2xx, so the keyword wildcard half proves nothing
  - L45-60 "Search wildcard" on the keyword index (`query=field:dynamic cate*`, URL-encoded space) has no hits assertion (only the generated default); the text half asserts hits[0]._id == 'doc' (L98). Note the keyword value is "dynamic categorisation" (one token) while the query puts the field on `dynamic` only; the intended query is probably `field:"dynamic cate*"` (src/query_dsl.cc:710-731 handles `*` on a keyword term)
  - L6 H2 "Search keywords and text with wilcards" typo (cosmetic)
## Rewrite scope

One section per area docs-site-refresh must cover. "Pages" are the existing pages the area touches, "In code, on no page" is what the code has that no page documents, "Write from" is the source of truth. Pages another open quest changes are marked (see "## Owned by other quests"); the refresh runs after sortable-encoding (D45), so the value-encoding text is written once.

### 1. Architecture

- Pages: `architecture.md`, `architecture/dependencies.md`, `architecture/internals.md`, `basics.md`, `about.md`, `welcome.md`. `internals.md` names ten paths that do not exist (`src/server/http*`, `src/url_parser.*`, `src/worker.*`, `src/server/base_*`, `src/booleanParser/`, `src/logger.*`, `src/msgpack.h`, `src/geospatial/htm.cc`); `dependencies.md` counts 59 Kronuz libraries where `CMakeLists.txt` declares 56 and omits strata, prospect, queryparser, analysis, snowball, unicode, task, namegen and constexpr-phf.
- In code, on no page: the engine seam (`src/engine/`, strata behind prospect) and what crosses it; the control loop and per-processor HTTP loops; requests as tasks that yield at 200 us slices; the `WorkPool` for device work; reader views (a search reads a published view, never holds its index); lazy WAL replay at the first writable open; the index registry and lease; the DataStorage blob store; the shutdown sequence; `:metrics` and the reactor-lag probe; the 35 s stop bound and exit codes 65/70/75.
- Write from: `ARCHITECTURE.md` (its "Clustering" and "Serialization" sections are themselves stale: check before copying), `docs/mechanisms/{strata-engine,requests-on-the-reactor,engine-write-path,reader-views,wal-replay,index-registry-and-lease,control-plane,wide-search,lua-bounds,text-queries}.md`, `docs/design/xapiand-on-strata.md`, strata `docs/ARCHITECTURE.md`, prospect `docs/ARCHITECTURE.md`, `CMakeLists.txt` for the library list (regenerate the table from it).
- Keep the value-encoding paragraph for after `sortable-encoding` lands (`docs/mechanisms/value-encoding.md`).

### 2. Features

- Pages: `index.mdx`, `about.md`, `welcome.md` (feature claims), `basics.md`. `welcome.md` says "Highly Available Distributed": the cluster is removed (single node).
- In code, on no page: wide search (`SEARCH /a,b/`, `_all`, globs over path segments, nested indexes, batches, per-index weighting, caps and 429/503), HTTPS, Lua scripting with bounds, collapse, stored-blob streaming past 16 MiB, null and empty-value types, terms-only fields and their range limits, 53 options, the Prometheus `:metrics` endpoint, `--legacy-ids`, admin verbs.
- Write from: `docs/status/behavior-changes-audit.md` (49 changes, the user-visible list), `[Unreleased]` in `changelog.md`, `docs/mechanisms/*`, strata `docs/PLAN.md` for what is out (cluster, `_cjk_words`, `?nearest`) and what returns (`more-like-this`, `cjk-words`, 7b).
- State the single-node scope once, in one place, and link it from the pages that mention the cluster (`basics.md`, `upgrade.md`, `contributing.md`).

### 3. Quick-start

- Pages: `quickstart.md`, `manipulation.md`, `exploration.md`, `next.md` (delete or fold into `exploration.md`), `basics.md`. The three examples are true; `quickstart.md` says the "twitter index" where the indexes are `twitter/user` and `twitter/tweet`; `exploration.md` links `reference-guide/query-dsl` (page is `reference-guide/search/query-dsl`).
- In code, on no page: the default `PUT` answers 204 with no body unless `--echo`/`?echo` (the examples in `documents/*.md` show echoed bodies); `#docid`, `#shard`, `#weight`, `#rank`, `#percent` appear only with `?comments`; where the data directory is (`-D`, `/var/db/xapiand`), the default port (8880, `config.h.in:302`), how to stop the node.
- Write from: `src/server/search_views.cc:938-985,2162-2250` (echo, pretty, human, comments), `src/opts.cc`, the docs E2E (`harness/run_all.sh e2e`) as the check that every example runs.

### 4. Installation and building

- Pages: `installation.md` (current), `building.md`, `upgrade.md`, `benchmarks.md` (build lines). `building.md`: CMake minimum is 3.12 (`CMakeLists.txt:26`) not 3.14; OpenSSL 3 (TLS is ON) and libuuid are build requirements it omits; the option table lacks TLS, LUA, UUID_*, FUZZ, CRASH_POINTS, ENGINE_CENSUS, RANDOM_ERRORS, CHECK_IO_FDES, CHECK_SIZES; `ninja check` needs `BUILD_TESTS=ON`.
- In code, on no page: packages (RPM x86_64/aarch64, DEB amd64/arm64 for Debian 13+/Ubuntu 24.04+, FreeBSD 14 pkg, multi-arch Docker image) are documented; the Docker entrypoint's variables (`XAPIAND_UID`, `XAPIAND_GID`, `XAPIAND_NAME`, `XAPIAND_DATABASE`) and data volume are not.
- Write from: `CMakeLists.txt:46-70` (options), `:388-404` (libuuid, OpenSSL), `Dockerfile`, `contrib/docker/xapiand/entrypoint.sh`, `.github/workflows/{release,freebsd,bottles,ci}.yml`.
- `release` owns the name in the formula, the image and the packages; write these pages so the name is one substitution.

### 5. Usage tutorials

- Pages: `tutorials/home.md` (a stub that says "there aren't many tutorials yet"), `tutorials/spatial-search.md` (current; an orphan: not in the sidebar), `exploration.md`.
- In code, on no page: there is no tutorial for aggregations beyond one example, for backup and restore, for scripting, for wide search over many indexes, for schemas and strict mode, or for TLS.
- Write from: the E2E fixtures in `tests/` and `docs/public/assets/{accounts,cities,fruits}.ndjson`; each tutorial request must run in the docs E2E.

### 6. Configuration and options

- Pages: `options.md` (everything it says is true; it covers 24 of 53 options and is not in the sidebar), `reference-guide/wal.md` (`--writers`), `reference-guide/schemas/*` (`--strict`).
- In code, on no page: 29 options on no page: `--bind-address`, `--max-clients`, `--max-files`, `--schema-pool-size`, `--schema-versions-size`, `--scripts-cache-size`, `--resolver-cache-size`, `--uuid`, `--admin-commands`, `--log`, `--uid`, `--gid`, `-P`, `-L`, `--force`, `--color`, `--no-color`, `--iterm2`, `--no-pretty`, `--no-echo`, `--no-human`, `--no-comments`, and the build-conditional `--random-errors-*`; the environment (`XAPIAND_DATA_PATH` for stop words and `mime.types`, `XAPIAND_LAG_PROBE`, `XAPIAND_ENGINE_CENSUS` on a census build); the barrier switch when `barrier-warning` lands (A2: "documented with the other options").
- Write from: `src/opts.cc:110-272` (the list and the help strings), `src/opts.h`, `src/manager.cc:117`, `src/main.cc:966`; generate the table from `xapiand --help` so it cannot drift.
- Removed options are fatal at startup (`harness/removed_options_check.sh`); `options.md` should list them once, with the cluster ones the changelog omits (`--replicas`, `--replicators`, `--remote-clients`, `--remote-servers`, `--replication-clients`, `--replication-servers`, `--discoverers`, `--database-stall-time`).

### 7. HTTP API reference

- Pages: `reference-guide/api.md`, `documents/*` (9), `indexes/*` (7), `schemas*` (19), `search*` (aggregations 21, query DSL 22). 20 are `wrong`: see the table. Worst: `documents/update-api.md`, `documents/versioning.md`, `documents/index-api.md`, `indexes/backups.md`, `schemas/dynamic-typing.md`, `schemas/field-types/geospatial-type.md`, `search/aggregations/bucket.md`, `query-dsl/leaf-queries/casting-values.md`.
- In code, on no page: verbs `COMMIT`, `FLUSH`, `QUIT` (these two need `--admin-commands` and the root URL, else 405), `CHECK`, `COUNT` (only in an example), `UPSERT`, `PATCH` (a patch document, `documents/update-api.md`), `WAL`, `DUMP`/`RESTORE` of one document, `OPTIONS`, and the 405/501 answers for `LOCK`, `COPY`, `MOVE`, `OPEN`, `CLOSE` and the rest;
- In code, on no page: the search URL parameters (`q`, `query`, `sort`, `metric`, `icase`, `limit`, `offset`, `check_at_least`, `collapse`, `collapse_max`, `spelling`, `synonyms`, `volatile`, `selector`), the body keys, the response shape (`count`, `total`, `took`, `hits`, `aggregations`), `?commit`, `?version`, `?echo`, `?comments`, `?selector`;
- In code, on no page: limits and statuses: body 16 MiB (413), chunked `RESTORE` 413, query 100 levels / 4,096 clauses / 256 phrases per AND / DSL 45 levels, aggregations 65,535 buckets and 16 levels, 503 for a busy index or too many readers, `Retry-After`;
- In code, on no page: schema properties: `_dynamic`, `_strict`, `_store`, `_recurse`, `_ignore`, `_positions`, `_spelling`, `_ngram`, `_cjk_ngram`, `_bool_term`, `_partials`, `_error`, `_accuracy_prefix`, the seven `*_detection` flags, the detection order, the 245-byte term limit, the 10-level partial-path limit, geospatial shapes (`_convex`, `_multi*`, `_geometry*`, `SRID=`), type-name aliases;
- In code, on no page: query DSL: the queryparser grammar inside text strings (boolean words need `_and`/`_or`), `_synonym` and `_max` modifiers, `"field": "*"` and null queries, range string forms, the 50-term cap of `_partial` against uncapped wildcard and fuzzy, `_near`/`_phrase` refusing wildcard and fuzzy operands, the accepted cast words.
- Write from: `src/server/search_views.cc:187-600` (verbs and routes) and its views; `src/database/handler.cc`; `src/database/schema.cc` and `src/reserved/*.h`; `src/query_dsl.cc`; `src/aggregations/*`; `docs/status/behavior-changes-audit.md` rows 7-35; queryparser README (the grammar table); `docs/mechanisms/{text-queries,numeric-range-terms,wide-search,lua-bounds}.md`.
- Generate the verb table and the reserved-word lists from the sources above; hand-copying them is how `geo_point` and `_date` got in.
- Do not document `more_like_this`, `?nearest`, `?fuzzy` or INFO terms until `more-like-this` lands (it owns those paragraphs).

### 8. Operations: dump, restore, upgrade, backup

- Pages: `reference-guide/indexes/backups.md` (wrong), `upgrade.md` (stale), `reference-guide/wal.md` (stale), `options.md` ("Stopping the node"), `docs-upgrade-page`'s new `upgrading.md`.
- In code, on no page: `DUMP` answers JSON, YAML or MsgPack only (`Accept: application/x-ndjson` answers 406); `RESTORE` takes NDJSON/MsgPack streamed with `Content-Length` (a chunked one answers 413), a JSON array buffered up to 16 MiB, disables the WAL and autocommit and commits once; its response is `{prepared, processed, indexed, total, items, took}`;
- In code, on no page: a data directory with Xapian files exits 65 and names the first file (`src/main.cc:660-670`), and the message links `https://kronuz.github.io/Xapiand/upgrade/#upgrading-from-100-alpha9` (`src/xapian_data_guard.h:104`): the heading and file name stay, or the constant changes in the same change;
- In code, on no page: WAL: replay is lazy (an index's shards replay at the first writable open), `WAL /index/` returns the log, the log carries no spelling or synonym writes;
- In code, on no page: `CHECK`, `COMMIT`, `FLUSH` (`?databases`, `?clients`), `QUIT`; `:metrics` and the 35 `xapiand_*` series; shutdown (35 s, second signal, exit 75) is on `options.md` only;
- In code, on no page: `DELETE /index/` answers 501 (not implemented): there is no way to delete an index over HTTP, and no page says so outside the two `indexes/delete-api.md` banners.
- Write from: `src/server/search_views.cc:1497-1790` (flush, quit, commit, dump, restore, wal, check), `src/main.cc:640-712,840-895`, `src/xapian_data_guard.h`, `docs/mechanisms/wal-replay.md`, `harness/cli_roundtrip_check.sh`, `harness/restore_stream_check.sh`.
- `docs-upgrade-page` writes the alpha.9/alpha.10 path; `backups.md` links to it rather than restating it.

### Site infrastructure (not a page)

- `docs/src/lib/sidebar.mjs` says "GENERATED by scripts/gen-sidebar.mjs from ../docs/_data/docs.yaml. Do not edit by hand"; neither exists. It is hand-edited now. Pages in no sidebar: `about.md`, `help.md`, `options.md`, `reference-guide/wal.md`, `reference-guide/schemas/field-types/geospatial-type/htm.md`, `tutorials/spatial-search.md`, `tests/schema/basque-language.md`, `tests/schema/stem-language.md` (`index.mdx` is the splash).
- `docs/astro.config.mjs` `editLink.baseUrl` ends `/edit/master/docs-astro/`; the directory is `docs/`. Every "Edit this page" link is a 404.
- `docs/README.md` describes a prototype ("196 pages", "Jekyll", `npm run convert`, `scripts/convert-docs.mjs`); `docs/scripts/` does not exist.
- Broken internal links: `exploration#sample-dataset` from 16 aggregation pages (the heading is "Loading the Sample Dataset", slug `loading-the-sample-dataset`); `schemas/dynamic-typing.md` `#text-detection`, `#uuid-detection` (no such headings); `schemas.md` links namespaces to `indexing-mode`; `field-types.md` `#time`, `#time-delta`; `stop-words.md` `love-and-hate-modifiers`; `exploration.md` `reference-guide/query-dsl`. Relative links (`delete-api`, `../versioning`, `..#structuring`) depend on the trailing-slash policy and were not resolved; the refresh's link check settles them (docs-site-refresh A4).
- E2E: `docs_to_postman.py` runs every fence of the whole tree (396 requests; 172 in `tests/`), nothing checks the count, so a broken fence shrinks coverage silently; `harness/e2e_check.sh` is dead (needs a 61 MB baseline that does not exist, describes a Xapian-era run); `docs/dev/testing.md:34,117-124`, `harness/README.md:1-16` and `harness/e2e_assert.py` still cite `docs/_docs`, `docs/tests` and `{% comment %}`. Two orphan fixtures run but cannot be reached. Merge candidates: `tests/schema/{stem,basque}-language.md`; `tests/querydsl/{fuzzy,partial,wildcard}.md`; `reference-guide/schemas/dynamic-field-names.md` into `namespaces.md`.
- Most examples show echoed bodies, `#docid` and `#shard`: the docs E2E turns `echo` on for every request (`docs_to_postman.py` adds `commit`, `volatile`, `echo`), so the rendered examples differ from a plain request. The refresh should show what a reader's request returns, or say that the examples are echoed.

## Unverified

Claims the audit could not settle from code, each with the probe. All run against a node started as `xapiand --port 8880 -D <empty dir>` on master unless said otherwise; "E2E" is `harness/run_all.sh e2e`.

- `reference-guide/api.md:L203-204` | `PUT /`, `POST /` and a search at `/` answer 400 | probe: `POST /` with a body; `SEARCH /?q=a`; `COUNT /` against a running node (the PUT / case is in the page's e2e). Code path: expand_paths yields one empty path, Endpoint() throws ClientError "empty or outside the data directory" (src/endpoint.cc:240-248)
- `reference-guide/documents/delete-api.md:L32-37` | DELETE ignores ?version | probe: `PUT /t/d/1` then `PUT /t/d/1` (version 2), then `DELETE /t/d/1?version=1`: code says 204; docs say 409
- `reference-guide/documents/store-api.md:L293-311` | `UPDATE` with Content-Length: 0 and a content type removes that content | probe: `UPDATE /assets/Lenna` with `Content-Type: image/jpeg` and an empty body, then `INFO /assets/Lenna` (data length 3); code path update(): body NIL/empty branch data.erase(ct_type), src/database/handler.cc:640-650
- `reference-guide/indexes/create-api.md:L28-33` | which characters and what length are refused in a created index name | probe: `PUT /a%20b/`, `PUT /a.b/`, `PUT /a:b/`, `PUT /<244-byte name>/` against a running node; expect 400 only for the cases in src/index_path.h
- `reference-guide/schemas/dynamic-typing.md:L46-49` | `{"age":20}` is typed `integer` (e2e asserts it; five more e2e pages assert `integer` for JSON ints) | probe: the code types MsgPack POSITIVE_INTEGER as `positive` (src/database/schema.cc:5098-5107); whether the JSON loader hands over 20 as positive or integer is outside src/. Probe: `PUT /t/1 {"age":20}` then `GET /t/._schema.age._type`
- `reference-guide/schemas/dynamic-typing.md:L129-138` | after the fix, numeric strings type as `text` | probe: `PUT /t/` with `{"_schema":{"numeric_detection":true}}`, `PUT /t/1 {"my_float":"1.0","my_integer":"1"}`, `GET /t/._schema.my_float._type`
- `reference-guide/schemas/dynamic-typing.md:L101-110` | `datetime_detection:false` plus "2015/09/02" types `text` | probe: `PUT /t/ {"_schema":{"datetime_detection":false}}`, `PUT /t/1 {"create":"2015/09/02"}`, `GET /t/._schema.create._type`
- `reference-guide/schemas/explicit-types.md:L52-53` | a negative `age` for a `positive` field is rejected | probe: `PUT /test_explicit_types/2 {"name":"x","age":-1}` expect 4xx (code: value.u64() on a negative, src/database/schema.cc:5421-5425; the catch that maps a msgpack type_error to 400 was not found)
- `reference-guide/schemas/indexing-mode.md:L92-103` | a `field_values` keyword matches nothing in a term query | probe: docs E2E request as written (SEARCH /test_index_mode/ with `{"_query":{"code":"AB-123"}}`, count 0); query_dsl.cc has no TypeIndex branch, so it follows from no terms being written (src/database/schema.cc:5337)
- `reference-guide/schemas/dynamic-field-names.md:L27-28` | schema gains one namespaced `tags` field however many keys appear | probe: index two docs with different keys under `tags`, then `GET /test_dynamic_names/._schema.tags` and compare the field list
- `reference-guide/schemas/namespaces.md:L52-57` | `style.clothing.pants:"khakis"` count 10 | probe: harness E2E on the bank fixture (fixture-dependent, not derivable from code)
- `reference-guide/schemas/foreign-schemas.md:L29-30` | `.schemas` index is created if missing and the schema doc id is a uuid | probe: `PUT /headlines/` request as written, then `GET /.schemas/319b4e5e-41af-4906-b3cd-cce91502dda3`
- `reference-guide/schemas/field-types/uuid-type.md:L18` | "Document IDs are of this type by default" | probe: `POST /t/` (no id) and `PUT /t/abc`: check `_id` type via `GET /t/._schema._id._type`
- `search/aggregations.md:L45-49` | "result may be approximate when running on longs whose absolute value is greater than 2^53" | probe: PUT docs with integer 9007199254740993 and 9007199254740995, SEARCH {"_limit":0,"_aggs":{"s":{"_sum":{"_field":"n"}},"m":{"_max":{"_field":"n"}}}}; on arm64 long double is 64-bit, on x86_64 80-bit, so the digits differ by platform
- `search/aggregations/bucket.md:L609-617` | multi-index "terms aggregation will promote non-decimal numbers to decimal" | probe: create index a (age integer) and b (age float), SEARCH /a,b/ with {"_aggs":{"x":{"_terms":{"_field":"age"}}}}; the schema comes from one handler get_schema (search_views.cc:1967-1968), probably not per index, so the warning may be stale
- `search/aggregations/bucket/range-aggregation.md:L17-22` | open ends on integer fields | probe: code reads: open bounds default to numeric_limits<long double>::min() (the smallest positive number, not lowest) and max(), then are converted into int64/uint64 pairs (bucket.h ranges_i64, configure_i64): negatives may miss a `{"_to": 0}`-style open-lower range and max() -> int64 is undefined on x86_64. Probe: index {"n": -5}, {"n": 3} (integer field), SEARCH {"_limit":0,"_aggs":{"r":{"_range":{"_field":"n","_ranges":[{"_to":10},{"_from":0}]}}}} on x86_64 and arm64; expect "..10" to hold both and "0.." to hold 3
- `search/aggregations/bucket/values-aggregation.md` | `_values` over a `text` field and keyword case | probe: PUT {"t":"Red Car","k":"Red"} and {"t":"red","k":"red"} with t text, k keyword; SEARCH {"_limit":0,"_aggs":{"a":{"_values":{"_field":"t"}},"b":{"_values":{"_field":"k"}},"c":{"_terms":{"_field":"k"}}}}; strata-engine-callbacks.md:48 says text `_terms` is 400 but `_values` "unchanged"
- `search/aggregations/metrics/variance-aggregation.md` | one value | probe: index one doc, SEARCH {"_limit":0,"_aggs":{"v":{"_variance":{"_field":"n"}}}}; metrics.h divides by (_count - 1) = 0, expect NaN/inf or a 400 and say what the JSON shows
- `search/aggregations/metrics/count-aggregation.md:L32-34` | "number of cities" in Indiana | probe: SEARCH the example plus a _values on contact.city under the same query and compare the bucket count with 17
- `search/aggregations/bucket/filter-aggregation.md` | `_term` on a keyword field with different case, and on a text field | probe: SEARCH {"_limit":0,"_aggs":{"f":{"_filter":{"_term":{"favoriteFruit":"Strawberry"}}}}} (bank sample); the filter compares serialised slot values (bucket.cc:56-80)
- `reference-guide/search/query-dsl/compound-queries/logical-operators.md:L15` | `_not` over two or more subqueries means "match none of them" | probe: `SEARCH /bank/` with {"_query":{"_not":[{"gender":"female"},{"eyeColor":"brown"}]}} with &limit=0 and compare total against 1000 minus |female OR brown|; code reading says it is NOT(female AND_NOT brown) = males plus brown-eyed females.
- `reference-guide/search/query-dsl/leaf-queries/casting-values.md:L98` | `_date` / `_time` casts | probe: `SEARCH /bank/` {"_query":{"age":{"_date":"2020-01-01"}}} expect 400 "Invalid operator: _date" (code: src/query_dsl.cc:365-388 has no case for it)
- `reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md:L39` | only `days` survives | probe: compare `SEARCH /bank/` {"_query":{"personality":"days"}} total against the page's 333 for "these days are few and far between"
- `reference-guide/search/query-dsl/leaf-queries/text-queries/love-and-hate.md` | a lone `-word` | probe: {"_query":{"personality":{"_and":"-ambitious"}}} on a field without `_language`: queryparser answers "Syntax: <expression> NOT <expression>" (400) when BOOLEAN is on and there is no stop list (queryparser:queryparser.cc:1250-1257); with a stop list or without BOOLEAN it retries as plain words. Probe both with `_and` and a plain string.
- `reference-guide/search/query-dsl/leaf-queries/text-queries/wildcard.md:L7` | `{"_wildcard":"ba?"}` | probe: on `favoriteFruit` and `personality`: expect it to be a prefix `ba?` (no single-char wildcard) vs `b?a`/`ba?n`, which glob.
- `reference-guide/search/query-dsl/leaf-queries/text-queries/fuzzy.md:L42-47` | `word~2` anywhere | probe: the `~n` digits form is accepted only at the end of the text (queryparser:queryparser.cc:631-636): probe "banna~2 apple" vs "apple banna~2".
- `all E2E blocks of the 22 pages` | expected counts and id lists on the bank corpus (strata ordering, multi-index not involved) | probe: the docs E2E (campaign check docs-site-refresh e2e) settles them; nothing here was run.
- `tests/comments/ignore-comments.md:L12` | trailing comma in `{"_type": "uuid",}` and the many other trailing commas on every page are accepted | probe: `curl -XPOST localhost:8880/x/ -d '{"a":{"_type":"keyword",}}'` expects 2xx; harness/README.md says the rapidjson relaxed-JSON flags are load-bearing
- `tests/datatypes/date-type.md:L8-95` | 'yyyy/mm/dd hh:mm:ss' -> '2015-01-01T12:10:30', ISO Z kept as '...Z', epoch 1420070400.001 -> '2015-01-01T00:00:00.001' as stored values | probe: `PUT /dt/doc {"date":"2015/01/01 12:10:30"}` then `GET /dt/doc`, then the Z and epoch bodies on the same field; or `harness/run_all.sh e2e`
- `tests/datastructures/objects.md:L716` | `behavior AND the` on a `_language: en` text field returns 0 (stop word not dropped at query time) | probe: `SEARCH /test/complex_object/ {"_query":{"_and":[{"personality":"behavior"},{"personality":"the"}]}}` expects total 0; check after the queryparser change (query-string grammar changed, but this body is DSL)
- `tests/**/*:INFO assertions (25 requests)` | `values` keys sets, e.g. ['0','1'] for term-only fields, are what the strata engine returns (value slots 0 id, 1 version, plus the field slots; slot 2 "shards" is absent) | probe: `harness/run_all.sh e2e` on the master binary (the gate e2e_assert.py is the settle); last recorded GREEN was 576/576 in docs/status/engine-core.md (A3), before nesting and the later changes
- `tests/schema/range-search.md:L120` | field_terms range returns exactly 20,100,180 | probe: the E2E; numeric_range_check.py covers value-slot ranges but not this over-cover count
- `architecture.md:L"capped near 34 GB"` | a volume's size cap | probe: read storage library docs or run: write past 34 GB (not run); probe: grep the cap in Kronuz/storage
- `building.md:L"Clang >= 14 / GCC >= 13"` | minimum compilers | probe: build with GCC 12 and Clang 13 in a job (or read the CI matrix in .github/workflows/ci.yml)
- `upgrade.md:L"decodes about 120x faster"` | cuuid v6 decode speed | probe: Kronuz/cuuid benchmark, or `cuuid` bench target
- `upgrade.md:L"Index settings and explicit schemas are set again ... before the restore"` | a RESTORE without a prior index create keeps the dump's schema or not | probe: `RESTORE /x/` on a fresh node with a dump carrying a schema, then `GET /x/:schema` (harness/restore_schema_check.sh)
- `manipulation.md:L"one second delay (refresh interval)"` | a write is searchable 1 s later | probe: PUT then SEARCH in a loop on a running node, time to first hit
- `benchmarks.md:L16-25` | every figure | probe: re-run harness/loadtest.py on master (closing-benchmark owns it)
- `help.md:L17,L35` | the IRC channel `#Xapiand` on irc.freenode.net and the `@germbravo` Twitter account are live | probe: ask the owner; no code probe settles it

## Owned by other quests

Pages an open quest changes. The refresh does not edit these paragraphs until the quest lands (or coordinates with it); `docs-site-refresh` also depends on `sortable-encoding` (D45).

| Quest | State | Pages and what it changes | Race to avoid |
|---|---|---|---|
| `docs-upgrade-page` | active | new `upgrading.md` (file set `docs/src/content/docs/upgrading.md`, `docs/src/content/docs/**`, `docs/public/assets/**`); `reference-guide/indexes/backups.md` (restore procedure, links to it) | `upgrade.md` already holds the 0.40.0 and alpha.9 story and is the page `src/xapian_data_guard.h:104` links, anchor `#upgrading-from-100-alpha9`. Two upgrade pages is the risk: decide `upgrade.md` is replaced or redirected, and keep the anchor or change the constant |
| `sortable-encoding` | planned, critical | `changelog.md` (A11); text on how numbers and dates are stored: `reference-guide/schemas/field-types/{numeric,date,geospatial}-type.md`, `architecture/internals.md` ("Value encoding" row and the monotonic-encoding paragraph), `copyright.md` (the GPL note), `about.md` (the MIT claim), `tests/schema/range-search.md` and `tests/datastructures/arrays.md` (pinned value-slot bytes; xxh32 slot numbers do not change) | the refresh is after it by D45; until then leave the encoding paragraphs |
| `more-like-this` | planned, high | `changelog.md` (A8); `reference-guide/documents/info-api.md` (INFO terms return, A2); `reference-guide/search/search-api.md` (`?nearest`, `?fuzzy`, A4); `reference-guide/search/query-dsl.md` (`more_like_this`, A3); E2E examples | do not document these as absent or present; the audit found no page that says either |
| `barrier-warning` | planned, high | `changelog.md` (A4); `options.md` (A2: the switch, documented with the other options) | the refresh rewrites `options.md`: add the switch row last, or leave the file to the quest |
| `release` | planned, critical | the name everywhere: `index.mdx`, `about.md`, `welcome.md`, `help.md`, `installation.md`, `quickstart.md`, `building.md`, `copyright.md`, `architecture.md`, every `/Xapiand/` internal link (the base path is `astro.config.mjs:49`), the Homebrew formula and image name, `changelog.md` (A5: cut `[Unreleased]`) | write the refreshed pages with the name and the base path as one substitution |
| `closing-benchmark` | blocked | `benchmarks.md` (file set lists it) | the page is Xapian-era; the refresh must not rewrite it with other figures |
| `cjk-words` | blocked (after the release) | `reference-guide/schemas/field-types/text-type.md`, the CJK note; `tests/schema/define-prefix.md` | `_cjk_words` stays undocumented until it returns |
| `license-closure` | planned | `reference-guide/documents/store-api.md` (A1: the Lenna images become a generated sample, `docs/public/assets/Lenna.*` removed), `tutorials/spatial-search.md` (A5: the map screenshot `el_cerrito.png`), `copyright.md` | the store-api examples change with it; the E2E examples that use Lenna move |
| `gate-prep-followups` | planned | `reference-guide/api.md` (A1: `HEAD /:stats`, `/:schema`, unknown commands at the root; adds a docs E2E example), `reference-guide/wal.md` (A5: `:wal` streams, answers a shard path) | the root-command paragraph of `api.md` |
| `phrase-expansion` (and the Xapiand quest it opens) | planned | `query-dsl/leaf-queries/text-queries/{phrase,near}.md` (wildcards, fuzzy and partial words inside phrases) | write them as "words only" now and leave the extension to that quest |
| `xapiand-final-release`, `phase7b` | planned, blocked | file set `docs/**`; no page text; 7b brings the cluster back, which the "single node" sentences (`basics.md`, `upgrade.md`, `contributing.md`) anticipate | none for the refresh |

## Code questions the audit raised

Not docs defects: places where the code, not the page, may be wrong or may surprise a reader. The refresh documents what the code does today unless the lead rules otherwise.

1. `DELETE /index/` answers 501 Not Implemented (`src/server/search_views.cc:1479-1491`); `reference-guide/api.md:156-167` and `documents/delete-api.md:17-19` say it deletes the index. There is no way to delete an index over HTTP in this tree.
2. A strict new index must carry `number_of_replicas` (`MissingTypeError`, `src/index_resolver_lru.cc:573-585`) although the cluster is gone and the value has no effect (`reference-guide/indexes/create-api.md:55`); six examples in `tests/namespace/namespace.md` carry it for that reason.
3. Range aggregation open bounds default to `numeric_limits<long double>::min()` (the smallest positive number, not the lowest) and are narrowed into int64 pairs (`src/aggregations/bucket.h` `ranges_i64`, `configure_i64`): negative values may miss an open-lower range (probe in "## Unverified").
4. `_not` over two or more subqueries builds `match_all AND_NOT (A AND_NOT B)` (`src/query_dsl.cc:287-294`, array branch `:413-418`), which is not "none of them" as `logical-operators.md:15` says (probe in "## Unverified").
5. `DELETE` ignores `?version` (`src/server/search_views.cc:833-851`, `src/database/shard.cc:1448-1462`) while `documents/delete-api.md:32-37` promises a 409; `_method` in a script is always the empty string (`src/database/handler.cc:300`, a `TODO`).
6. The `variance` of one value divides by zero (`src/aggregations/metrics.h`, `n - 1`); the `_missing` example in `search/aggregations/bucket.md:472-509` passes only because the parameter is ignored.
7. The binary's refusal message links `https://kronuz.github.io/Xapiand/upgrade/#upgrading-from-100-alpha9` (`src/xapian_data_guard.h:104`); `docs-upgrade-page` writes `upgrading.md`.
