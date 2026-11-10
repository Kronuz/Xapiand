# docs-site-refresh--architecture: journal

State: waiting on jobs 261110-034547-0a4dbb
Quest: .campaign/xapiand/quests/docs-site-refresh.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pages rewritten from code at base d02dd743d: `docs/src/content/docs/architecture.md`,
`architecture/internals.md`, `architecture/dependencies.md`. No `rest` or `e2e:begin` blocks on these
pages, before or after, so the docs E2E is untouched (D47). Jobs: site build + link check (A4), A3 grep.

## Next

1. Read job 261110-034547-0a4dbb (site build + link check); fix any broken link from or to the architecture pages, rerun, then set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the "Audit rows" table below: every row of `docs/dev/docs-site-audit.md` for `architecture.md`, `architecture/dependencies.md`, `architecture/internals.md` fixed, with the source of each fixed claim |
| A2 | not touched: these pages carry no request blocks before or after (`git grep -c "e2e:begin" -- 'docs/src/content/docs/architecture*'` is empty) |
| A3 | 261110-034548-e17d04 at 14207d0be: `git grep -niw xapian -- 'docs/src/content/docs/architecture*'`, 0 matches, exit 0 (also 261110-034448-665693 at 07bb9134e, 0 matches) |
| A4 | 261110-034547-0a4dbb at 14207d0be (queued): `npm ci`, `npm run build`, then a link check over `dist/` that fails on any broken link from or to the architecture pages and lists the rest; 261110-034448-ae5c89 at 07bb9134e cancelled (superseded) |
| A7 | [manual] the owner reads `architecture.md` before landing (lead) |

## Audit rows (A1)

Sources are at the branch base (d02dd743d) unless a library and commit is named; library commits
are the pins in `CMakeLists.txt`.

| Page, audit row | Fix | Source |
|---|---|---|
| architecture.md L53 `src/server/http*` does not exist | Link removed; internals' table names `src/server/search_views.cc` (route table), `search_service.*`, `http_utils.h`, `mime_types.*` | `src/server/search_views.cc:548-602` |
| architecture.md "Storage ... capped near 34 GB" (and its Unverified row) | Volumes are per-shard blob volumes `docdata.*`, 32 GiB each; the index is strata's `strata/` dir | `src/database/shard.cc:82,1031-1043`; `src/database/pool.cc:497-504`; storage@9f14417 `README.md:22` (2^32 8-byte units) |
| architecture.md: no WorkPool | "The process" table, "A request" | `src/database/pool.cc:565-568` (one WorkPool, `threads=0`); threadpool@eb4751a `work_pool.hh:726,758-762` (0 = hardware concurrency); `src/manager.cc:284`; chore classes `src/engine/chores.h:34-38` |
| architecture.md: no control loop | "The process", "Stopping" | `src/manager.cc:178-185` (one reactor, Tasks, Periodic, Debounce), `:543-560` (runs on the main thread), `:506-512` (minute housekeeping), `:1034-1179` (Stop 1-5, `_Exit(EX_TEMPFAIL)`), `:156-168` (35 s bound) |
| architecture.md: no engine seam census | internals "The engine seam" (seam + census paragraph) | `src/engine/shard.h:89-178`; `CMakeLists.txt:70` (ENGINE_CENSUS OFF); `src/engine/census.h:25-29` |
| same row's scope ("In code, on no page"): HTTP loops per processor | "The process" | `src/opts.cc:188-189,282,380`; `src/manager.cc:901-908` (SO_REUSEPORT on Linux), `:918-966` (N reactors), `:989-1000` (start on READY) |
| same: requests as tasks, 200 us slices | "A request" 4 | `src/server/search_views.cc:487,3133` |
| same: admission | "A request" 1, Limits | `src/manager.cc:950,961` (1000); http@c813531 `http_asio.h:1062-1066` (per reactor, 503 + Retry-After 1); `src/server/search_views.cc:3071-3085` (256 demoted) |
| same: bodies | "A request" 2, Limits | `src/server/search_service.h:50` (16 MiB); `src/server/search_views.cc:2827` (64 KiB decode chore), `:1663-1677` (RESTORE flags, `when_all` overlap); `src/database/pool.h:157` (4 GiB stream) |
| same: reader views, read-your-writes | "Indexes on disk", "Visibility" | `src/engine/shard.h:89-93`; `src/database/shard.cc:1331-1336` (`engine_for_read` -> `fresh`); `src/database/pool.cc:360-370` (`fresh`: turn + refresh only when dirty) |
| same: index registry and lease | "Indexes on disk" | `src/database/pool.h:26-30,329` (strata catalog of prospect indexes, pins, turns), `:67` (DB_TIMEOUT 30 s); `src/database/pool.cc:327-356` (`take_turn`, busy -> 503); strata@5d742b2 `include/strata/core/catalog.hh:366-372` (sweep 1 s, demote 30 s, close 600 s), `:28` (S3-FIFO); `src/opts.cc:50,158` (pool 200); `src/main.cc:441-487` (lowered to fds) |
| same: lazy WAL replay at open | "Durability" | `src/database/shard.cc:453-500` (replay in the open chore, then commit); `src/database/wal.cc:153-170` (from the committed revision); `src/engine/strata/shard.cc:715-730` (`replay_applies`); `src/database/wal.cc:259,657` (quarantine) |
| same: DataStorage blob store | "Indexes on disk", write path 1 and 4 | `src/database/shard.cc:1016-1048` (blobs to `docdata.*`), `:648` (commit syncs storage first); `CMakeLists.txt:55` (DATA_STORAGE ON) |
| same: shutdown sequence, 35 s, exit codes | "Stopping", Limits | `src/manager.cc:156-168,1058-1179`; 75 `:1179`; 70 `:755,888`; 65 `src/main.cc:668,704` (statuses left to options.md, linked) |
| same: `:metrics` and the reactor-lag probe | internals table "Metrics"; dependencies (prometheus-cpp, `/:metrics`) | `src/server/search_views.cc:497,531,568` (`GET /:metrics`); `src/manager.cc:1268,1324-1344` (`xapiand_reactor_lag_seconds`), `:115-120` (`XAPIAND_LAG_PROBE=0`) |
| architecture.md: write path (claims written) | "Indexes on disk", "The write path" | placement `src/database/handler.cc:515-527,1870-1894`, `src/database/utils.cc:93-99` (FNV-1a 64); 5 shards `src/opts.cc:43,152`; 1-shard path `src/index_resolver_lru.cc:729`; `.xapiand/node` `src/manager.cc:346,363`; `.xapiand/indexes` holds settings and schema `src/index_resolver_lru.cc:176-200`, `src/database/schemas_lru.cc:309-314`; apply + log under the turn `src/database/shard.cc:735-787`, `src/database/pool.h:120-123`; log line queued, not written, at ack `src/database/wal.cc:848-856,1097-1115` (`DB_SYNCHRONOUS_WAL` has no setter: `git grep DB_SYNCHRONOUS_WAL -- src` lists only `flags.h`); fsync debounce `src/opts.cc:71-75`; autocommit `src/opts.cc:65-68`, `src/control_plane.h:179-214`; commit `src/database/shard.cc:636-668`, coalesced `:550-620`; strata commit protocol strata@5d742b2 `docs/ARCHITECTURE.md` "Write path" item 10; RESTORE without log `src/server/search_views.cc:1663`, `src/database/shard.cc:1296-1300` |
| architecture.md: search path | "The search path" | `q=`/`query` `src/server/search_views.cc:2427-2433`; boolean-parser `src/query_dsl.cc:959`; queryparser `src/engine/text.cc`; `_query` `src/reserved/query_dsl.h:30`; patterns in a chore `src/server/search_views.cc:2286-2324`; batches `src/database/handler.cc:1296-1316,1466-1488`, `src/opts.cc:160-164`; BM25 `src/engine/strata/search.cc:153`; `search_task` with the slice `:730-734`; spy, sort, collapse `:700-728` |
| architecture.md: limits | Limits table | `src/database/utils.h:143`, `utils.cc:191-192` (200 levels, ClientError); `src/engine/query.h:94-98`; `src/aggregations/aggregations.h:45-46`, `aggregations.cc:399-400`, `exception.h:29`; views `src/manager.cc:266-277`; Lua `src/lua/processor.h:50-62` |
| architecture.md: one node | "One node" | `src/index_resolver_lru.cc:196-200` (`number_of_replicas` stored); `src/node.*` keeps the local identity only; strata@5d742b2 `docs/DISTRIBUTION.md` |
| architecture.md: manifest log | Not described: strata's commit does not use it at the pin | strata@5d742b2 `docs/mechanisms/manifest-log-codec.md:1-3` ("Nothing uses it yet"); `git grep manifest_log 5d742b2 -- include src` hits only its own header |
| dependencies.md "59 standalone Kronuz libraries" | 57 Kronuz + 8 third-party | `awk '/FetchContent_Declare/{f=1} f&&/GIT_REPOSITORY/{print;f=0}' CMakeLists.txt \| grep -o 'github.com/Kronuz/[A-Za-z0-9_.-]*' \| sort -u \| wc -l` = 57; third-party: asio `CMakeLists.txt:746`, CLI11, cppcodec, RapidJSON `:484-486,987-989`, LZ4 `:992`, prometheus-cpp `:1018-1021`, Lua `:1034-1039`, libyaml `:1085` |
| dependencies.md: tables miss strata, prospect, queryparser, analysis, snowball, unicode, task, namegen, constexpr-phf, libyaml | All in the tables (plus http-log, boolean-parser) | `CMakeLists.txt:487,730-737,765,772-773,787,797,838-841,1085` |
| dependencies.md: lists fantasyname, perfect-hash, stash, uinteger_t, times, bloom-filter, lazy, queue, radix-router | Removed from the tables; stash, uinteger_t, radix-router shown as transitive in the tree | `CMakeLists.txt:943-946`; scheduler@d57e8fc `CMakeLists.txt:40`; base-x@a8e56f8 `CMakeLists.txt:10`; http@c813531 `CMakeLists.txt:28` |
| dependencies.md "random: xorshift128+" | "Random helpers over one per-thread generator" (no algorithm named) | random `README.md` |
| dependencies.md "Lua 5.4.7, sol2" | Lua 5.4.7 through the C API with the metering patch; no sol2 | `contrib/lua/lua-version.cmake:24`; `CMakeLists.txt:1030-1039`; `git grep sol2 -- src` empty |
| dependencies.md tree | Regenerated from each library's CMake at its pin; test and benchmark deps left out | strata@5d742b2 `CMakeLists.txt:33,41`; prospect@850d4e8 `:10-26`; http@c813531 `:28-73` (cpp-httplib bench-only `:99-107`); msgpack@88aab56 `:13-36` (comparators bench-only `:162-179`); logger@f61d8df `:48` (fmt, quill bench-only `:125-134`); scheduler@d57e8fc `:40`; storage, reactor, task, queryparser, analysis at their pins |
| dependencies.md: purposes | Rewritten from each README where the old line was vague (stringified, allocators, url-parser, math, iterators, epoch) | each library's `README.md` |
| dependencies.md: system libraries | zstd (required), libuuid (required), zlib, threads, OpenSSL 3 with TLS | `CMakeLists.txt:53,298,363,388,404,960-963` |
| internals.md L30 `src/database/{wal,data,shard}` (current) | Kept, one row each | files exist |
| internals.md L32 `src/server/http*.{cc,h}`, `src/url_parser.*` | `src/server/search_views.cc` etc.; URL parsing is url-parser | `src/server/`; `CMakeLists.txt:696` |
| internals.md L33 `src/worker.*`, `src/server/base_*` | Control-loop row: `src/manager.*`, `src/control_plane.h`, `src/shutdown_wait.h`; the loops are reactor's | `src/manager.cc:178-185` |
| internals.md L34 `src/booleanParser/` | Query row: `src/query_dsl.*` (`q=` through boolean-parser), `src/engine/text.*` (queryparser), `src/engine/query.*` | `src/query_dsl.cc:32,959`; `src/engine/text.cc` |
| internals.md L37 `src/sortable_serialise.*` and the monotonic paragraph (sortable-encoding's) | Row lists `src/serialise*`, `src/cast.*`, `src/length.*`; the paragraph states only the order property | ruling R2 |
| internals.md L38 `src/logger.*` | `src/log.h`, `src/logger_fwd.h`; the engine is the logger library | `src/log.h:40-56` (categories as L_NOTHING) |
| internals.md L55 `src/msgpack.h` | MsgPack is the msgpack library's | `CMakeLists.txt:660` |
| internals.md L85 `src/geospatial/htm.cc` | htm library + `Serialise::trixel_id` | `CMakeLists.txt:930`; `src/serialise.cc:772-780` (htobe56) |
| internals.md "~2,790 L_* call sites" (unverified) | Count dropped | ruling R3 |
| internals.md storage write-cursor invariant | Dropped: the storage library's, not Xapiand's | ruling R4 |
| internals.md WAL replay `revision == db_revision` | Rewritten: per-line version stamps | `src/engine/strata/shard.cc:715-730` |
| internals.md new invariants | turn and view rule, the turn covers apply + log line, reads take no turn, index path bytes | `src/engine/host.h:36-43`; `src/database/pool.h:120-123`; `src/database/pool.cc:360-370`; `src/index_path.h:27-30` |
| internals.md reserved keys `$`/`_` | `_` only | `src/reserved/reserved.h:26` |

## Result

<When ready for review.>

## Rulings I made

- R1: No request blocks added to the architecture pages: they explain the system, and what a request can
  show (limits, verbs) belongs to the reference pages A5 covers. The pages stay outside the docs E2E.
- R2: The value-encoding row and paragraph (sortable-encoding owns them) keep only the property any
  encoding keeps (byte order = value order) and name no routine or layout, per the lead's prompt.
- R3: Counts no job measured (the `L_*` call sites) are dropped, not refreshed.
- R4: Invariants that belong to a library (storage's write cursor and bin alignment) leave this page; the
  library documents them.
- R5: The engine census, a developer build option, gets one paragraph on internals.md, not the overview.
- R6: The durability paragraph says an acknowledged, uncommitted write can be lost in a crash: the log line
  is queued to a writer thread, not written, when the write is acknowledged
  (`src/database/wal.cc:848-856,1097-1115`), and `DB_SYNCHRONOUS_WAL` has no setter in `src/`.
  `options.md` ("A write answered with a 2xx is in the log") holds for a clean stop only: for the lead.
- R7: The name and the `/Xapiand/` base stay as they are (the release quest substitutes them everywhere).
