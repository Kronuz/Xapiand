---
title: "ChangeLog"
---

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---
## [Unreleased]

### Added
- **`_sort` takes the order as a string**: `{"balance": "desc"}` and
  `[{"age": "asc"}, {"balance": "desc"}]`, as Elasticsearch writes them.
- **`:metrics` reports how late each event loop runs**: the `xapiand_reactor_lag_seconds` histogram, one
  series per HTTP loop and the control loop, from a probe that wakes every 1 ms while the loop serves requests
  and 10 times a second when idle. `XAPIAND_LAG_PROBE=0` starts the node without the probes.
- **A stored blob past 16 MiB uploads in a stream**: a `PUT` of a `stored` document (any content type that is not JSON, YAML
  or MsgPack) with a `Content-Length` over the 16 MiB buffered-body cap is written to the data storage as it arrives, in O(chunk)
  memory, and the document is indexed once the blob is durable. The blob is stored uncompressed; past 4 GiB it answers `413`,
  and a `POST` without an id in the URL answers `400`.

### Changed
- **The node's own work runs on its control loop.** The minute housekeeping (idle closes, merges to publish), the debounced autocommit of a write that asked for no commit,
  the signal reader and the node setup are tasks of the control loop that wait on timers and events; the cleanup thread and the committer threads are gone.
  Autocommit times are as before (a commit 1 s after the last write, 3 s while writes continue, and by 8 to 10 s after the first).
  `xapiand_committers_running` and `xapiand_committers_queue_size` count the watch tasks and the indexes waiting for a commit.
- **Concurrent first uses of a schema or of an index's settings read it once.** A burst of requests for a new index used to read its schema metadata and its settings once each; they now wait for
  one read. Stop words and `mime.types` load at startup, before the listeners open.
- **A cancelled read leaves its index's queue at once.** A search or get whose client went away, or that a shutdown stops, no longer
  waits for the index's turn to refresh (up to 30 s) before it ends. A write that was accepted still completes.
- **The HTTP service stops within the shutdown's budget.** At shutdown the requests are cancelled, the sockets of stalled clients close
  after a grace, and the loops stop one grace later. The grace is half of what the 35 s have left after 5 s for closing the indexes (15 s
  when nothing else took time), so a write that waits for its index's turn (up to 30 s) is let finish: it applies, and its client is
  answered if the turn came within the first grace. Stalled clients can hold the stop for that grace.
- **A shutdown waits at most 35 s after the clients are done, in all.** The HTTP service, the autocommits and the closing of every index share that bound. Past it the node logs what it abandoned (the requests in flight per loop, and the indexes
  whose turn is held or that are still in use), flushes the write-ahead logs and exits with status 75; an index that cannot close within the bound (a hung disk, for instance) ends the stop the same way, where
  the wait for it used to have no limit. A second `SIGINT` or `SIGTERM` ends the wait at once. The steps are logged in order
  (`Stop 1` to `Stop 5`). A write that was answered with a 2xx is in the log. See [Stopping the node](/Xapiand/options#stopping-the-node).
- **A node that cannot set itself up exits.** A taken port, or any failure while the servers start, is logged and ends the process with status 70 (it used to log "Servers listening",
  fail on the bind, and stay up serving nothing).
- **A Lua script runs in slices, in its own environment, with bounds.** A script yields its event loop every 10,000 instructions
  instead of holding it, so a long script no longer delays the requests behind it. Each call has its own `_doc`, globals and
  copies of `string`, `table`, `math` and `utf8`: calls of one script no longer share globals. A call that passes 100,000,000
  instructions, 500,000,000 units of work (what its string, table and pattern calls touch: bytes, steps, elements, charged per
  call and yielded in slices), 64 MiB of live data or 10^6 instructions in a callback that cannot yield answers `400`, and
  `pcall` cannot catch it. A single library call that would touch more than 16,000,000 units (about 20 ms: a quadratic pattern over
  a 1 MiB subject, `upper` over 8 MiB) answers `400` too, so no call holds a request loop longer than that. A script's result is converted back within 16 MiB of strings and 262,144 value units (an object is 8)
  (a bigger one answers `413`; the whole document counts, so a script over a document of more than 262,144 value units answers `413` even when
  it touches one field), the Lua stack holds 200,000 slots instead of 1,000,000 (`table.unpack` over about 200,000 elements raises "too many
  results to unpack"), and any error a script raises answers `400` instead of storing the document untransformed. `table.sort` is written in
  Lua (a stable merge sort) so comparisons yield. `load`, `dofile`, `loadfile`, `collectgarbage`, `string.pack`, `string.unpack`,
  `string.packsize`, `string.dump`, to-be-closed variables and the `__gc`, `__close` and `__mode` metamethods are no longer
  available to scripts, and `print` logs at most 4 KiB per call. A script saved under a name now replaces the cached script of the
  same name; a foreign script is cached apart from the local ones and re-read when its document changes (within a second).
- **One pool: `WorkPool`.** The `ThreadPool` family is gone from the threadpool library, so the thread-policy
  tags that selected nothing (`fsynchers`, `wal_writer`, and the others) are removed; the fsync and WAL writer threads are
  named as before. A shard's turn wait is `task::async_mutex::lock(token)` with a `task::deadline` built from the host's
  timer (the `lock_until` it replaces is gone from task): the same `turn_timeout`, the same "The index is busy" error.
  The libraries move: threadpool `65bc0bc` to `eb4751a`, scheduler `2bb0a8f` to `1e5ccc9`, logger `801105c` to `2356791`,
  task `8cad661` to `b406766`, strata `501f943` to `3f564dc`, prospect `465ee7d` to `bd78224` and strict-stox `b947d77`
  to `1f9190c`.
- **The Kronuz libraries move to their heads of 2026-11-02**, and `CMakeLists.txt` declares only the 57 that Xapiand
  includes directly (it declared 64): the libraries pin the rest at their own heads, so stash, uinteger_t, times,
  lazy, queue, bloom-filter and radix-router are no longer declared here (from
  [`cpp-libraries/docs/UPGRADING-2026-10.md`](https://github.com/Kronuz/cpp-libraries/blob/main/docs/UPGRADING-2026-10.md)):
  boolean-parser `ac248d1` to `85110af`, datetime `569521c` to `5dd9155`, string-similarity `cbc0a79` to `594e6d6`,
  htm `11e0c46` to `bb5e01d`, msgpack `75d13ce` to `e0b4696`, fs `a7a7693` to `11067ed`, storage `e4c5746` to `f3a38fc`,
  url-parser `4424e25` to `def41f8`, fantasyname `a87942a` to namegen `3eb14ce`, scheduler `2cbea95` to `2bb0a8f`,
  logger `90094b5` to `801105c`, http-parser `178f873` to `c27615d`, and the rest of the family, with strata at
  `501f943` and prospect at `465ee7d`. The results that change are listed below. strata's fusefilter moves to
  `0665ee0`, whose build writes other filter bytes, so **the segment version bumped** (`0x8000000C` to `0x8000000D`, and
  strata reads only the current version): an index written by an older pre-release build is not read, and must be
  indexed again.
- **Boolean query strings (`q=`, `query=`) group by precedence, and malformed ones answer `400`.** An implicit `OR`
  obeys precedence: `a AND b c` is `(a AND b) OR c` (it was `a OR (b AND c)`), and `NOT a b` is `(NOT a) OR b`. An
  implicit `OR` before `NOT` or `(` parses (`a NOT b` answered `400`). Text that was accepted by dropping an operand
  or a parenthesis answers `400` with the parser's message: `a NOT`, `a ()`, `(a` (`'(' not expected`), and
  an operator without operands. All 10 query strings of the documentation's requests parse to the same
  queue as before; `harness/boolean_query_check.py` holds the classes.
- **Dates that `datetime` no longer reads are text.** A new field whose first value is a time with a fraction
  dot and no digits (`2020-01-01T10:00:00.Z`) or a fraction of more than 62 digits is typed `text` and indexed;
  it was typed `datetime` and the write answered `400`. `2020-12-15||+12M` is `2021-12-15` (it was an error), a
  negative hour, minute or second is out of range, and a timestamp before 1970 with a fraction converts right.
  The 7,330 strings of the documentation's requests and fixtures (101 dates) type the same;
  `harness/datetime_detection_check.py` holds the classes.
- **Sorting by a string metric scores by the standard definitions.** `Jaro`, `Jaro-Winkler`, `Sorensen-Dice` and
  weighted `Levenshtein` are corrected, and a phonetic metric encodes both operands: sorting by `soundex` against
  `DICKSONX` puts `DICKSONX`, `DIXON`, `DWAYNE` first (it began `DWAYNE`, `DUANE`, `DIXON`). Of 8 metrics and 3
  reference strings over 18 names, only the `soundex` orders moved.
- **Geospatial ids of points near a cell edge are the same on every build.** `htm` turns floating-point contraction
  off: a build that fuses multiply-adds (Apple silicon, `-mfma`) put 0.7% of random points, and 8 of the
  1,000 cities of the documentation, in the neighboring trixel (`-117.1858759 34.5008311` was `11168355159896371`
  and is `11168355159896369`). A build that does not fuse is unchanged; reindex the points of an index written by a
  fusing build.
- **A document that repeats a key keeps one entry.** `{"a":1,"a":2,"b":3,"c":4}` is `{"a":2,"b":3,"c":4}` (it was
  `{"a":2,"b":3,"b":3}`). None of the documentation's bodies or fixtures repeats a key.
- **Paths follow Python's `os.path.normpath`.** An empty data directory resolves to `.` (it was `/`), `a/../b` is `b`,
  `./a` is `a`, `../a` stays `../a`, and `//x` is `/x`; the storage volume of an empty base path opens
  `./vol.0`. A request path `a/b/..` is the path `a`, and `../a` is the path `..` with the id `a`.
- **Node names and random draws differ for a seed.** `namegen` gives every alternative of a choice the same
  probability (the first and the last came up half as often), and `random` is xoshiro256++.
- **The HTTP parser rejects what request smuggling relies on** (a bare LF, obs-fold, a `Transfer-Encoding` other than
  exactly `chunked`, a malformed `Content-Length` or chunk size): the server closes the connection without a reply (a bare LF
  request answered `200`, and `Transfer-Encoding: chunked, identity` answered `405`).
- **Requests run on the HTTP loops, and a long search shares its loop.** A request is a task on the loop that accepted it: it
  suspends for a turn, a read or a chunk of its body instead of holding a thread, and a search yields at window
  boundaries (a 200 us slice), so one heavy query no longer delays the light ones behind it. A loop with 256 requests
  waiting for their next slice answers `503` with `Retry-After: 1` to new ones. A search stops at its next slice when its client
  goes away. `RESTORE` parses its stream on the pool beside the indexing of the previous chunk, and a document body over 64 KiB
  is decoded on the pool; `FLUSH` waits for the closes without holding a thread.
- **A data directory with Xapian indexes is refused at startup.** A node started on files from
  1.0.0-alpha.10 or earlier (`iamglass`, `flintlock`, `postlist.*`, `*.glass`) exits with status 65 and names the
  first one, instead of starting with empty strata indexes beside them. Dump the indexes with that release and restore
  them here (see [Upgrading](/Xapiand/upgrade#upgrading-from-100-alpha9)).
- **A request that cannot get its index's turn in time answers `503`.** Every operation on an index (a write,
  a commit, a refresh before a read, the timed commit and the idle close) takes that index's turn, and waits for
  it for at most 30 s. The old pool made a request wait on a condition variable for a free shard, and answered
  the same error when it ran out of time; now the wait is bounded by the one turn, not by a pool of copies.
- **A search no longer holds its index.** A search reads a published view of each index it names, so a long search
  does not delay a write, a refresh or a commit to the same index (it used to hold the index's lock for its whole
  run). A search sees the index as of the moment it started.
- **Too many searches of one index at once answer `503`.** Each index admits a fixed number of concurrent readers
  (16 per HTTP loop plus the pool's workers); a search that finds the index at its cap answers `503` instead of waiting.
- **A search over many indexes reads them in batches.** A search holds at most `--concurrent-search-indexes` shards open
  at once (by default a loop's share of the database pool) and merges each batch's best hits as the batch ends, so a
  search over thousands of indexes is served. Hits, order, weights, totals and aggregations do not depend on the batch
  size. `--max-search-indexes` (100,000, at least 1) is a cap on a runaway pattern: a search over more, counting what a
  wildcard expands to, or whose patterns read more directory entries than that to find them, answers `400` with the
  limit in the message. `:metrics` reports `xapiand_search_pinned_indexes`
  and `xapiand_search_pinned_indexes_peak`.
- **A search and a count take `_all`, wildcards and exclusions in the index list.** `SEARCH /_all/`, `/test*/`,
  `/*test/` and `/test*,-test3/` search the indexes on the node that match; before, a pattern was taken as a name and
  found nothing. The names are listed in a chore of the work pool, not on the loop. A pattern that matches nothing
  answers `200` with no hits. A search names at most 32 patterns, and the walk is cancelled when the client goes away.
  The node reads its data directory for one such search at a time: at most `--search-walk-queue` (8) wait behind it, and
  a search over that answers `429`; one that waits `--search-walk-wait` (10 s) for its turn answers `503`, both with `Retry-After: 1`.
- **Index patterns are globs over path segments, and indexes nest.** `*`, `?` and `[...]` match inside one segment and
  never a `/`; a segment `**` stands for any number of segments (`**/some-??` finds `some-ab` at any depth, and a
  pattern may hold several `**`); `_all` is `**`. The search goes only into the directories a pattern can still match
  below, so `logs/2026/*` reads `logs` and `logs/2026` and nothing else, and it goes into index directories too: `twitter`
  and `twitter/user` can both exist, and creating `tenant` does not hide `tenant/a` from `_all` or a prefix. Every
  entry read, at any depth, counts against `--max-search-indexes`. Before, a `*` matched a `/` too (`test*` found
  `test1/old`); write `test*/*` or `test*/**` for that. An index name with a `?` or a `[...]` class in a search is a pattern now,
  as one with a `*` was.
- **A relevance weight is read against the statistics of its own index.** A document weighs the same in a search of its
  index alone and in a search over many indexes, as Elasticsearch weighs a shard's documents by default; before, the
  indexes of one search were weighed as one, so adding an index to a search changed the order of the others' hits.
- **A search past 4.29 billion document ids works.** The id of a hit across the shards is 64 bits wide; a search over
  100,000 indexes of more than 42,949 documents each used to be refused.
- **A write that asks for a commit and waits too long for it still succeeds.** The write is applied and logged; its
  commit is left to the timed commit, so a client is not told to retry what was done (an `add` without an id would
  have been stored twice).
- **The device work of an index runs on a shared pool of workers.** Opening an index, writing a segment,
  committing, closing and reading or writing a stored blob run as jobs of the index's own queue on one pool of
  workers, so a slow disk for one index no longer holds the threads that serve the others, and a write does no
  I/O of its own while it holds the index.
- **`collapse_max` above 1 returns the best documents of each group** on the strata engine: for each group,
  the best `collapse_max` documents of the window. The Xapian engine returns lower-ranked documents of a
  group when a better one fell outside its window (the bank's `?collapse=age&limit=5` answers accounts 6,
  11, 16, 21 and 1000 on Xapian and the five best, 1 to 5, on strata). `collapse_max` 1 is the same on both.
- **Queries have size limits.** A query nests at most 100 levels of operators and holds at most 4096
  terms and operators; the query DSL allows 45 levels of nested clauses, and a JSON, YAML or MessagePack
  body 200 levels. A YAML body may not use anchors or aliases (`&name`, `*name`): they answer `400`.
  An AND (or FILTER) joins at most 256 phrases and `_near` groups. A query with more than
  4,096 clauses, or nested past the depth cap, is refused with `400` and the limit in the message.
  Elasticsearch's defaults are 30 levels and 4096 clauses.
- **`_terms` counts a field's stored values** (as Elasticsearch's terms aggregation reads doc
  values), once per document and distinct value. Keyword buckets keep their case (`Red` and
  `red` are two buckets).

- **`_terms` and `_values` keep a bounded number of buckets** while they collect: at most about
  `_limit * 1.5 + 10` (Elasticsearch's `shard_size`, settable with `_shard_size`), dropping the
  buckets with the lowest counts. Counts can fall short of the true counts when a field has more
  distinct values than that. They are refused (400) over geo fields.

### Fixed
- **Every integer ID is its own document.** Integer IDs past 2^32 could share a key: a `PUT` of 4294967297
  and one of 4311744512 left one document. A numeric ID's key is now its exact number, for every 64-bit
  integer and every float, and new integer IDs run up to 2^63 - 1 (2^53 for float IDs) instead of 2^32 - 1.
  The ID keys change on disk: an index written before needs a dump and a `RESTORE`.
- **`--restore` into a new, empty data directory writes the index's settings.** The CLI restore resolved
  the index read-only, so `.xapiand/indexes` lacked its shard records and a node serving the directory answered
  `500` ("Inconsistency in number of replicas") or counted 0 documents. It now resolves the index as `RESTORE`
  does, from the dump's first object.
- **A shutdown no longer leaves requests waiting on a stopped loop.** A request still waiting for an index's
  turn, a chore or its body when the loops stop is now destroyed before the indexes close, which gives its turn
  and its hold on the index back first.
- **`RESTORE` of a dump no longer bumps one document per shard.** A record that carries `_version`, as every
  dump record does, was stored one version too high when it landed at docid 78 of its shard (ids 386 to 390
  of a five-shard dump).
- **A deeply nested or very wide query no longer crashes the node.** A text of thousands of nested
  brackets or tens of thousands of words, and a query DSL nested a few hundred levels, overflowed the
  stack of the thread that ran the search.
- **A nested MessagePack or YAML body, or a few hundred ANDed phrases, no longer crashes the node.** A
  MessagePack body of 800 nested arrays (about 1 KB), a YAML flow sequence 1,000 levels deep, and an AND of
  about 600 `"phrase"` or `_near` clauses overflowed the stack; they answer `400` now. A YAML body that is
  empty or a bare scalar no longer crashes the decoder either, and a YAML alias chain or "billion laughs"
  body no longer exhausts the stack or the memory: anchors and aliases answer `400` before any expansion.
- **A field path of 150 levels answers `400`**, not `500`, and a `_fuzzy` text of tens of thousands of words
  is refused in milliseconds instead of after seconds.
- **Nested bucket aggregations over a multi-valued field no longer exhaust the host.** A
  document is still counted once in each bucket it belongs to, as in Elasticsearch, so seven
  values nested six levels made 117,649 buckets (8 s, 200 MB). A search now holds at most
  65,535 live buckets (Elasticsearch's `search.max_buckets`) and aggregations nest at most 16
  levels deep; past either it answers 400 naming the limit.
- **A client error raised inside an aggregation or a sort on the strata engine answers 400**,
  as on the Xapian engine (it answered 500).
- **`_terms` on numeric, boolean and date fields** returned the accuracy terms of the index
  among the values; it now returns the values.
- **An empty data directory and an index path outside the data directory are refused.** `-D ""` used to mean the
  current directory and now exits with a usage error. An index path that is empty, `.`, `a/..` or escapes with `..`
  (`PUT /a/../doc`) answers `400`, and `--dump` or `--restore` of such a name exits with a usage error.
- **An absolute index path outside the data directory is refused.** `PUT /%2Ftmp%2Fx/doc` (an encoded slash first), a `_foreign` schema or script
  that names `/tmp/x/doc`, and `--dump` or `--restore` of `/tmp/x` used to create, read or delete an index at that path, outside the data directory.
  They answer `400` (a usage error on the command line). An absolute path inside the data directory still names the index it points at.
- **An index path with a NUL byte no longer reaches one level above the data directory.** `PUT /..%00/doc`, a `_foreign` schema or script
  link that names `..%00/x`, and the same through `--dump` or `--restore` passed the boundary check as a three-byte name and were opened by
  the system as `..`; with `--shards 1` one request aborted the node. A path with a NUL, a control byte, a backslash or malformed UTF-8 (an
  overlong `%c0%af`, a surrogate) now answers `400` (a usage error on the command line) instead of a `500` with an internal message.
- **`GET /`, `HEAD /`, `GET /:info`, `GET /:metrics` and `GET /.name` answer again.** They named the data directory itself as an index and
  answered `400 Index path '' is empty or outside the data directory`. The root names no index; a request that needs one (`PUT /`, a search) still answers `400`.
- **A damaged WAL's quarantine directory failure no longer aborts the node on macOS.** The `fs` library's `quarantine_files` formatted a
  `char[PATH_MAX + 1]` buffer in its log message and left it unterminated for a path of `PATH_MAX` bytes or more.

### Removed
- **`--committers`, `--fsynchers` and `--http-servers`.** The commit threads are gone (autocommits are tasks on the control loop), the fsync pool is sized from `--processors`
  (half of them, at least 1, at most 10), and the node runs one HTTP loop per processor: `--processors N` sets both (it was capped at 10 loops, now it is not).
  The `num_committers` key of the root `GET /` statistics and the `xapiand_http_clients_*`, `xapiand_committers_capacity` and `xapiand_committers_pool_size` gauges of `:metrics` are gone.
- **`--http-clients`, `--bulk-preparers` and `--bulk-indexers`.** The offload threads and the bulk preparer and indexer
  threads are gone; requests and the restore indexer are tasks on the HTTP loops, and device work is on the pool.
- **`--max-database-readers`**: an index is opened once and shared by every request; there are no pooled
  reader copies to cap.
- **The `_elite_set` operator** (query DSL, default operator and the engine's ELITE_SET). A query with it
  answers `400` ("Invalid operator").
- **The `terms` of `INFO` on a document** (every term the document indexes, with its frequency and
  positions). `INFO` still reports the document's id, version, data and values.
- **The `_terms` aggregation over an analysed `text` field**, which counted index words. A `_terms` on a
  field without stored values answers `400`; keyword, numeric, boolean and date fields are unchanged.
- **The `nearest` and `fuzzy` search parameters** (similar documents: the terms of the top hits
  expanded into a second query), with their `n_rset`, `n_eset`, `n_term`, `field` and `type` settings.
  A search that carries them is an ordinary search now. The `_fuzzy` query (edit distance) is
  unchanged.
- **The weight cutoffs** (`percent_threshold`, `weight_threshold`), which no request could set.
- **Query shapes that are not valid queries.** A `_synonym` over anything but words (a wildcard, a fuzzy
  word or a bracketed group in its text) and an operator that cannot take a list (`_scale_weight`,
  `_wildcard`, `_edit_distance` given an array) answer `400` and say the query is not valid. The Xapian engine
  used to search a `_synonym` over such text. A `_phrase` or `_near` over anything but words (a wildcard, a
  fuzzy word, a bracketed group) says so too, where it used to say that only leaf subqueries are supported
  "currently".
- **The Xapian engine.** strata is the only search engine, with no option to choose one (`XAPIAND_ENGINE` is
  gone), and an index lives in strata's files. Databases in Xapian's format are not read: dump them with
  the final Xapian-only release (the `xapiand` branch) and restore them into this one.
- **The cluster, until it returns.** This release runs on one node. The remote protocol, the replication
  protocol, primary election over Raft, multicast discovery and distributed search are gone, with their
  options (`--solo`, `--cluster`, `--discovery-port`, `--discovery-group`, `--discovery-interface`,
  `--xapian-port`, `--replica-port`, `--primary-node`, `--matchers`, `--flush-threshold`), the `cluster_name`
  and `nodes` of `GET /`, the `replication` log category, the replication and remote-protocol metrics, and
  the Kubernetes guide. The `.xapiand/nodes` cluster database is no longer created.
- **The `_cjk_words` field property** (ICU word-break segmentation of CJK text), which only Xapian served;
  `_cjk_ngram` is unchanged. ICU is no longer a dependency.
- **The `Xapian` and `ICU` entries of `GET /`'s `versions`**, and the `#xapian_query` debug field of a
  verbose search, now `#engine_query`.

## [1.0.0-alpha.10] - 2026-10-28

### Added
- **HTTPS for the REST API.** `--tls-cert` and `--tls-key` serve the API over TLS on
  `--port`, with forward-secret ciphers only, a connection cap (`--tls-max-connections`),
  and idle, header, and inactivity timeouts. Traffic between nodes still travels in clear,
  and the API still has no authentication; see [Options](/Xapiand/options).

### Changed
- **Builds need OpenSSL 3** by default, for the coming HTTPS support (`-DTLS=OFF` builds
  without it). The packages depend on it: `libssl3t64`/`libssl3` for the `.deb`,
  `openssl-libs` for the `.rpm`, `openssl@3` for Homebrew.
- **Index-time text analysis uses the Kronuz `analysis` and `snowball` libraries** instead of
  Xapian's term generator, with the same terms, and queries stem with the same Snowball
  stemmers. The newer Danish, Italian, and Norwegian stemmers drop apostrophe suffixes
  (`it's` now stems to `it`, `all'art` to `art`), so documents indexed before this change
  need reindexing to match such words. `cjk_words` still uses Xapian's term generator.
- **A numeric `_id`'s accuracy terms lead with `A` instead of `Q`,** so the id term is the only
  term of a document that leads with `Q`. Range queries over `_id` use the new terms, so an
  index with a numeric `_id` written before this change needs reindexing for them.
- **Text queries are read by the Kronuz `queryparser` library** instead of Xapian's query parser,
  on the host and once per query. The grammar is the same for the settings Xapiand uses, with
  two recorded differences: a lone exclusion such as `-a` is a syntax error (it used to be read as
  the word `a`), and the window of `+a b -c d` does not change because an exclusion stands between
  its words. `cjk_words` fields no longer split CJK text in queries.
- **Every library is pinned at its current release.** Each Kronuz library now pins its own
  dependencies, so Xapiand no longer pins the libraries its libraries fetch.

### Fixed

- **A `_near`, `_phrase` or `_elite_set` query over a boolean expression is a client error.**
  `{"_near": "(barack OR michelle) AND obama"}` corrupted the heap in the vendored parser; the
  text is now read without recursion, and an operator that cannot take its operand answers
  400 instead of 500.
- **Numeric range queries return every matching document.** The term walk behind a range over
  an integer, positive, floating, time or timedelta field skipped whole buckets (for example
  the 2e8 to 3e8 bucket, a range's own aligned start such as `[0, 0]`, and the coarse buckets
  of a wide range such as `[1, 3000000000]`), so documents in range were missing from the
  results. Open-ended ranges (only `_from` or only `_to`) no longer drop values beyond
  2x the coarsest accuracy, and an open-ended date range no longer drops dates before 1970 or
  after 2038. Index terms are unchanged: no reindex. On a field indexed with terms only
  (`_index: "field_terms"`), a range that terms cannot answer exactly (open-ended, or wider
  than 4096 coarsest buckets) now fails with a 400 that says so, instead of returning no
  documents; index the field's values to make such ranges exact.
- **A retried write no longer fails with a spurious version conflict.** After a failed write,
  the retry compared its own freshly assigned version against the stored one and returned 409.
- **`FLUSH` no longer stalls other connections.** It commits every open index, and it now does
  so on a worker instead of the connection loop that every client on that loop waits behind.
- **Concurrent `FLUSH` requests no longer crash the node,** and a request no longer waits for a
  free shard until its timeout when the shard was released or flushed while it waited.
- **Opening a shard opens it once.** Every open repeated itself eleven times, replaying the WAL
  each time.
- **German stop words now apply.** A duplicate entry in the German list made the stopper fail
  to build, so the first German text write failed and later ones skipped stop words.
- **Basque text fields work.** The `basque` language mapped to the unknown code `ue`, so
  every write to such a field failed with a 500; it now maps to `eu`.
- **Autocommits and WAL fsyncs requested during shutdown are no longer dropped.** When the
  committer or the fsync pool has already stopped (or cannot allocate), the commit or fsync
  runs right away on the calling thread.
- **Aggregations on a cluster count every document.** A node aggregated its own shards fully but
  only a sample of the remote ones, so `_doc_count`, `_sum` and `_avg` came back low and
  differed from node to node. Aggregations now check every match, whatever `_check_at_least`
  says.
- Builds with `-DLUA=OFF` retain the declaration of the existing scripting-unavailable stub, so the database handler compiles without Lua.

---
## [1.0.0-alpha.9] - 2026-10-17

The last release on Xapian before the move to strata.

### Fixed
- **Builds no longer break when a library changes.** Every fetched library, including the
  ones Xapiand's libraries fetch themselves, is pinned to a commit: each Kronuz library's
  default branch as of 2026-09-18, the last green build, and sol2 to v3.5.0-4. Building
  against moving `main` branches had broken Linux builds once cluster started depending
  on varint.

### Changed
- **Docs site:** the complete Discussions package is vendored, the comment widget is
  synced with its self-hosted Worker, dates render in UTC, and long titles wrap.

---
## [1.0.0-alpha.8] - 2026-07-11

### Changed
- **Faster reserved-word dispatch.** Perfect-hash dispatch uses constexpr-phf, and the
  schema's dispatch key and the stop-word hash are 64-bit, matching the word-at-a-time
  hash (18-51% faster than byte-at-a-time FNV on Xapiand's reserved names and field types).
- **Docs site** redeploys on each release, so its version badge stays current.

---
## [1.0.0-alpha.7] - 2026-07-10

### Fixed
- **The docs E2E suite** finds the relocated sidebar again.

---
## [1.0.0-alpha.6] - 2026-07-10

### Added
- **Docs site:** the version and CI status in the header, and an opt-in Discussions
  comments engine.

---
## [1.0.0-alpha.5] - 2026-07-10

### Changed
- **Document ids are UUIDv6 by default** (cuuid v6: timestamp-first, an 8-byte compact
  wire form). Existing v1 ids still decode; see the upgrade notes.

### Added
- **`--legacy-ids`** mints v1 ids as before.
- **The Python and JavaScript reference clients** understand v6 ids.

---
## [1.0.0-alpha.4] - 2026-07-09

### Fixed
- **ICU now links on Apple Silicon.** The Homebrew prefix was hardcoded to the Intel
  path, so `find_package(ICU)` failed and ICU was silently compiled out of the arm64
  build (and its bottle), losing Unicode word-boundary handling despite the formula
  depending on `icu4c`.

### Changed
- The Homebrew formula is **head-only** and never goes stale: the bottling workflow
  injects the released tag's url and a fresh source checksum instead of a pinned
  default. **Asio** is fetched by the build rather than declared as a formula
  dependency.

### Removed
- Stale contrib: the 2018 FreeBSD ports skeletons, the Django-Haystack backend, and
  a broken example script.

---
## [1.0.0-alpha.3] - 2026-07-09

### Changed
- Renamed **"indices" to "indexes"** throughout: the reserved metadata database is
  now `.xapiand/indexes`, and the reference guide follows. A clean break, with no
  fallback to the old name.
- The Python client (**xapiand-py**) was rewritten as a fully **async** (asyncio /
  aiohttp) library and bumped to 1.0.0. There is no synchronous compatibility layer.

---
## [1.0.0-alpha.2] - 2026-07-08

Second alpha: response-path efficiency, a crash-safe data store, a dependency-free
test harness, and the full release/CI pipeline. The documentation site moved to
Astro/Starlight.

### Changed
- **Zero-copy response logging**: the access log reads the response straight from the
  writer's buffer instead of copying the body and headers a second time per request.
  Requests and responses are logged through **Kronuz/http-log**.
- The data store writes a **crash-safe v2 volume format** (still reads v1).
- Documentation migrated from Jekyll to **Astro/Starlight**, with an Architecture
  section, D2 diagrams, a rewritten Lua scripting guide, and OS auto-detection on the
  quick-start.

### Added
- macOS **Homebrew bottles** (arm64 + x86_64 via Rosetta), **FreeBSD packages**,
  multi-arch **Docker** images, and **RPM**/**DEB** packages, all built on tag push.
- **ASAN + ThreadSanitizer** CI, with the doc-driven E2E run under both.
- A request **watchdog** that flags requests taking too long, and
  `harness/ci_status.sh` for one-glance CI status of a ref.
- The splash banner shows the build's **git short-hash** and version.

### Fixed
- Aggregations **fail clearly** on reserved-but-unimplemented types instead of
  silently returning nothing.
- FreeBSD build fixes (`<utility>` include, `__builtin_exp10` lowering); the version
  now derives from lightweight git tags.
- Bumped the `cuuid` JS dependency to close a `uuid` advisory
  (GHSA-w5hq-g745-h8pq).

### Removed
- **GoogleTest**: the unit tests run on a dependency-free harness and the legacy test
  tree was pruned (many suites revived: serialise, endpoint, fieldparser, geospatial
  HTM, and more).

---
## [1.0.0-alpha.1] - 2026-07-04

First 1.0 milestone (alpha): a broad modernization of the engine, the runtime, and
the build, on top of a de-vendored library tree. Six years and 151 commits after
0.40.0. Benchmarked native-vs-native against Elasticsearch and against 0.40.0; see
the [Benchmarks](/benchmarks/) page.

### Changed
- Migrated the vendored Xapian engine from the 1.5.0-dev snapshot to **2.0.0**
  (rebuilt as clean per-feature patches on pristine 2.0.0).
- Rebuilt the runtime on an **Asio-based reactor** and retired **libev** entirely:
  the HTTP server, the Xapian remote-backend protocol, the replication protocol, and
  the discovery/cluster plane (Bus + Raft) all run on the reactor now.
- Broke the monolith into a **tree of standalone libraries** consumed via
  FetchContent (msgpack, storage, traceback, logger, threadpool, scheduler,
  datetime, io, fs, system, htm, soundex, and ~20 more), replacing the vendored
  copies.
- Moved to **C++20**: `fmt` → `std::format`, `tclap` → CLI11, and the ChaiScript
  scripting engine → **Lua (sol2)**.
- Distributed search now runs through **one native multi-database `Enquire`** that
  overlaps the shard fan-out, replacing the serial shard-by-shard loop and the
  hand-rolled MSet merge.
- Data-store volumes and inline document data are written with **Zstandard**.
- Remote protocol synced to Xapian 2.0.0 (**v47**), as a non-blocking two-phase
  match the reactor can hold.

### Added
- **Apple Silicon / AArch64** build support.
- Native **multi-database overlapped shard fan-out** (see Changed) and a read-only
  **remote-database stats cache** that eliminates the per-query `MSG_UPDATE` storm.
- `--color=<mode>` log-color control and `--discovery-interface` selection.
- `RESTORE` streams its request body concurrently (O(buffer) memory).
- Content negotiation through the framework (`http::AcceptCache`).
- A full **test + benchmark harness**: differential E2E capture/check, multi-node
  cluster liveness checks, a load driver, a native Elasticsearch comparison, and a
  Remote-protocol proxy/decoder.
- Double Metaphone sort key.

### Fixed
- Distributed `query_id` made **unique per shard**, so shards co-located on one node
  no longer collide in the process-global pending-query map.
- A single **SIGINT drains in-flight HTTP requests** and shuts down the remote and
  replication layers gracefully.
- Data paths (stopwords, `mime.types`) resolve relative to the executable.
- Restored relaxed-JSON parsing, the `xapiand_http_requests_summary` metric, and the
  guarantee that an auto-generated `_id` is never left empty.

### Removed
- **libev**, entirely.
- The vendored copies of ~30 libraries (now external Kronuz-family deps).
- The hand-rolled per-shard fan-out and manual MSet merge, and Xapiand's bespoke
  HTTP parser / `Request` / `Response` classes (now the `http::` library).

---
## [0.40.0] - 2020-04-27

### Fixed
- Added limits to caches without it
- Added missing Z to represent UTC on datetimes values
- Fix update outdated cache schemas from local and foreign schemas
- Fix update cache of index settings when the schema is updated
- Increase node lifespan and heartbeat timeout

---
## [0.39.0] - 2019-11-19

### Fixed
- Serialization the get_mset steps due the concurrency produce a segmentation fault

---
## [0.38.0] - 2019-10-10

### Fixed
- Search offsets when merging results

---
## [0.37.0] - 2019-09-17

### Changed
- Raft election timeouts incresed by 2x

---
## [0.36.0] - 2019-09-16

### Added
- Added `--primary-node` option to fix a primary node

### Fixed
- Doing `GET /.nodes` only reads nodes internally

---
## [0.35.0] - 2019-08-20

### Changed
- Improve QueryDSL handling of wildcards and fuzzy

---
## [0.34.0] - 2019-08-20

### Changed
- Allow specifying partial using ** at the end

### Fixed
- Badly placed Wildcards
- Partials for keywords

---
## [0.33.0] - 2019-08-19

### Changed
- Send errors in script as 400
- Notify of database changes during periodic cleanups
- Limit shutdown retries to exit
- Prevent queries ending with <space><wildcard> to match all

### Fixed
- Fix scripts in strict mode

---
## [0.32.0] - 2019-08-05

### Fixed
- Using stemmer during queries

---
## [0.31.0] - 2019-08-01

### Changed
- No longer logging replication operations by default (it now requires `--log replicas`)

### Fixed
- Uncommon race condition while destroying clients
- Replicas no longer send db updated messages
- Always replicate databases from primary

---
## [0.30.0] - 2019-07-30

### Changed
- Adds default operator for queries parsed with the boolean parser

### Fixed
- Discovery protocol callback executed by appropriate thread
- Fixes race condition during shutdown

---
## [0.29.0] - 2019-07-25

### Changed
- **BREAKING**: Foreign schemas no longer accept selectors
- **BREAKING**: Global string fields indexed as `text` datatype
- QueryDSL using `_and` or `_or` also enable booleans

### Fixed
- Fix foreign schemas URIs
- Fixes issue #19
- Fixes issue #20

---
## [0.28.0] - 2019-07-24

### Changed
- **BREAKING**: `UPDATE` method no longer receives `upsert` query param
- **BREAKING**: Added `UPSERT` method
- **BREAKING**: Removed `MERGE` and `STORE` methods
- Restore always commits at the end

### Fixed
- Restoring of documents in a clustered index
- Commit in databases with remote shards (remote wasn't receiving the commit)
- Better handling of database flags (when changed)
- Uncompressed UUIDs representation was buggy

---
## [0.27.0] - 2019-07-17

### Changed
- **BREAKING**: `UPDATE` and `PATCH` don't create new objects, object must exist (or use `upsert` query param)
- **BREAKING**: `STORE` removed (use `PUT` or `UPDATE`)
- Updated xapian-core to latest 1.5 (git@00f69cf3928b44a756ddafcde248610f72babf62)

### Fixed
- Schemas not saving default schema as metadata some times
- Remote protocol matcher, for remote queries, now holds list for all pending queries

---
## [0.26.0] - 2019-07-12

### Changed
- **BREAKING**: Errors returned with `status` and `message` (no hash `#`)

### Fixed
- Fixed `_meta` producing errors when passed to `POST`, `PUT` or `UPDATE`

---
## [0.25.0] - 2019-07-11

### Added
- Added support for `_meta` inside schemas (to allow user to add custom meta
data associated with it)

### Changed
- **BREAKING**: Schema declaration no longer needs nested `_schema` -> `schema`
                node; it now only needs `_schama`
- **BREAKING**: Strict mode requires newly created indexes to declare `_settings`
                with at least `number_of_shards` and `number_of_replicas`.
- **BREAKING**: Foreign schemas no longer use `_endpoint`, it was renamed to
                `_foreign`.

### Fixed
- Aggregations (min/max) which have no elements could result in errors.
- Fix parser query params to qdsl for not operator

---
## [0.24.0] - 2019-07-10

### Changed
- **BREAKING**: Clustering: Added primary shard promotions
- Updated xapian-core to latest 1.5 (git@db790e9e12bb9b3ebeaf916ac0acdea9a7ab0dd1)
- Python client updated
- Quarantine (invalid) WAL and database files/directories when appropriate
- Only nodes marked as primary for shards get the replica, others simply erase
  (or quarantine) stalled databases.
- Unused server sockets are now fully closed during shutdown

### Fixed
- **BREAKING**: Clustering: Increased overall cluster stability
- Remote protocol: Race conditions for some used database objects after checkin
- Refactored node shutdown process
- Schemas LRU properly detect changes to schemas now checking only inside
  "schema" and "description"
- WAL had a bug which could lead to skipped slots (it now checks revision before
  writing lines and recovers after errors)
- Many solo mode (and CLUSTERING=off) fixes.

---
## [0.23.0] - 2019-05-24

### Changed
- **BREAKING**: Geo: Renamed `height` to `altitude`
- **BREAKING**: Geo: Renamed `lng` to `lon`

### Fixed
- Schema with nested arrays and objects

---
## [0.22.2] - 2019-05-16

### Added
- Support for 'float' as an alternative name to 'floating' datatype

### Fixed
- Undefined behavior in UUID, serializer and term generator
- Error when requesting invalid types

---
## [0.22.1] - 2019-05-16

### Changed
- Performance improvements with restore by launching parallel DocIndexers
- Queries using glob wildcards

### Fixed
- Nested arrays and objects in arrays
- Restoring for GCC compiled Xapiand

---
## [0.22.0] - 2019-05-10

### Changed
- **BREAKING**: Added _version to document data
- **BREAKING**: Non-hashed prefixes for fields
- Indexing optimizations (_id and _version values)
- RESTORE now doesn't check _version

### Fixed
- Concurrency issues
- Issues with document version checks
- Fuzzy Queries fixed

---
## [0.21.2] - 2019-05-06

### Changed
- Enabled Honey backend

### Fixed
- Issues when launching multiple nodes simultaneously
- Retry database initialization during node launch

---
## [0.21.1] - 2019-05-05

### Fixed
- Fix compiler warning about DropAction/GetAction "changing meaning"

---
## [0.21.0] - 2019-05-04

### Changed
- Default do check_at_least one when using aggregations
- Updated xapian-core, fixes issues with leaking sockets
- **BREAKING**: Added patches for xapian-core to handle parallelized queries

### Fixed
- Fix issues when starting servers simultaneously

---
## [0.20.1] - 2019-04-29

### Fixed
- Fix issue with aggregations returning invalid results at times

---
## [0.20.0] - 2019-04-27

### Fixed
- **BREAKING**: Remote Protocol issue which provoked messaged to get out of sync
- Issues schemas not being saved (unexpected "_id" errors)
- Detection of remote schema updates
- Certain cluster remote queries

---
## [0.19.7] - 2019-04-15

### Added
- Experimental parsing of YAML
- Postman collection for testing documentation requests

### Changed
- Require CMake 3.12 (again)
- Docker image using Alpine 3.9, GCC 8 and ICU

### Fixed
- Check for requests with invalid body
- Compilation with cluster disabled

---
## [0.19.6] - 2019-04-13

### Fixed
- Casts

---
## [0.19.5] - 2019-04-13

### Fixed
- Throttling of commits (exception was being thrown)

---
## [0.19.4] - 2019-04-13

### Fixed
- Compilation using cmake 3.5 (use static libraries instead of object libraries)
- Regression which slowed down bulk indexing

---
## [0.19.3] - 2019-04-12

### Added
- GCC 7 and GCC 8 support (and libstdc++)

### Changed
- Refactored pfh module
- Using ENUM() with minimal perfect hash functions (improves performance)
- Upgraded xapian-core to latest 1.5 (git@84ccecdfb8938b4011aac0bf9539c139204acd22)

---
## [0.19.2] - 2019-04-09

### Changed
- Colorized INFO
- Tracebacks with normalized address
- Require CMake 3.12

### Fixed
- Use nanosleep instead of usleep and libev checking for EINTR
- Fix warning about available files under OS X

---
## [0.19.1] - 2019-04-08

### Added
- Added CJK NGRAM and CJK words as parameters to 'text' datatype
- Enable fuzzy searches in queries: use '~'. E.g. "unserten~3" would expand to "uncertain"

### Changed
- Ignore XAPIAN_* environment variables
- INFO signal showing busy threads callstacks, when tracebacks are enabled
- All exceptions now contain tracebacks, when tracebacks are enabled
- Hash for resolving cluster node for indexes skipping slashes

### Fixed
- Fix ranges in query
- Fix restoring from an empty request
- Formatting applied to saved dates

---
## [0.19.0] - 2019-04-05

### Changed
- **BREAKING**: Remove support for \xHH in json

### Fixed
- Fixed errors during heavy writes/reads
- Fixed race condition in restore indexer

---
## [0.18.1] - 2019-04-02

### Fixed
- Fixed logging for verbose request/responses

---
## [0.18.0] - 2019-04-01

### Added
- The `?echo` query param returns the new/updated object

### Changed
- **BREAKING**: By default, write/update operations no longer return the object (see `?echo`)
- **BREAKING**: Removed `:schema`, schemas are now added/edited by using the
                Create Index API.
- Verbosity level 4 or higher (`-vvvv`) turns on `--echo`, `--pretty` and `--comments`

### Fixed
- Replication protocol now properly closing connections on errors

---
## [0.17.0] - 2019-03-28

### Added
- Set metadata
- Added `PUT` index

### Changed
- **BREAKING**: Removed `:touch`

### Fixed
- HTTP Mappings
- Schemas

---
## [0.16.1] - 2019-03-26

### Fixed
- Selectors were not working
- Mime Types

---
## [0.16.0] - 2019-03-26

### Added
- Added --pretty (and --no-pretty) options (for default `?pretty`)
- Verbosity also toggles "prettiness"
- HTTP method mapping + override
- TravisCI builds, thanks to Vitold S. (@vit1251)

### Fixed
- Fixed `_version` checks

---
## [0.15.0] - 2019-03-21

### Added
- Sharding support added
  + By default there are now five shards per index
  + Shards are put inside subdirectories with the shard number prefixed with `.__`

### Changed
- **BREAKING**: All schemas are now foreign by default, and get put inside the databases indexes
- **BREAKING**: Databases indexes directories work like shards and thus were renamed
                from `.xapiand/<node_name>` to `.xapiand/index/.__<node_idx>`

---
## [0.14.0] - 2019-03-12

### Changed
- **BREAKING**: Cluster directory renamed from `.cluster` to `.xapiand`;
- **BREAKING**: Indexes directories renamed from `.index/<node_idx>` to `.xapiand/<node_name>`
- **BREAKING**: Upgraded xapian-core to latest 1.5 (master)

---
## [0.12.2] - 2019-03-11

### Fixed
- Fix regression introduced in 0.12.1 during cluster bootstrapping

---
## [0.12.1] - 2019-03-11

### Fixed
- Fix requests resulting in Bad Gateway with stalled endpoints

### Changed
- Renamed options for logging from `--log-*` to `--log <setting>`

---
## [0.12.0] - 2019-03-10

### Fixed
- Aggregations with `_aggs` in different order
- JSON serialization of Infinite and NaN
- Fixed race condition during bulk indexing

### Changed
- **BREAKING**: Fields in responses renamed and nested differently:
                + `query.hits` moved to `hits`
                + `query.matches_estimated` renamed to `count`
                + `query.total_count` renamed to `doc_count`
- `MERGE` method is deprecated and will be removed, use `UPDATE` instead
- Default UUID mode is now encoded + compact

---
## [0.11.1] - 2019-03-06

### Fixed
- Fix Docker build

### Added
- Added support to routing key hints in the API, by using `routing` query param

---
## [0.11.0] - 2019-03-05

### Fixed
- Cluster replication is now functional, it was broken in 0.10.0
- Fix searching in "_id" field
- Fixed and documented Scripts and Foreign Scripts

### Changed
- **BREAKING**: `.index` shared by node
- **BREAKING**: Keyword datatype term prefix is now 'K'
- **BREAKING**: Guessed IDs are now strings also for numeric IDs
- **BREAKING**: URL drill selector now uses dot ('.') directly, so document IDs
                cannot have dots in them now.
- **BREAKING**: System-added fields in returned objects are no longer prefixed by '#'
- Never overwrite an existent database
- Index only adds new indexes during write operations
- Do not save empty data inside database
- Numeric IDs are auto-incremented when creating new documents with `POST`

### Added
- Added '*'' for using as multiple indexes

---
## [0.10.2] - 2019-02-20

### Fixed
- Docker build had missing symbols since 0.10.0
- Fix exit code on errors

---
## [0.10.1] - 2019-02-20

### Fixed
- Default database path is /var/run/xapiand

---
## [0.10.0] - 2019-02-20

### Added
- Added `_match_all` and `_match_none` to `:search` command
- Added the Elite Set Operator

### Changed
- **BREAKING**: Cluster using `.index` and `.cluster` databases
- **BREAKING**: Indexes paths (endpoints) no longer have a trailing slash
- Use OP_FILTER for terms in range queries
- Flattened range queries
- The default stopper during indexation is now `stop_stemmed`
- Numeric ranges are now properly calculating the filtering terms
- Geospatial queries are now calculating better the filtering trixels terms
- Default accuracies changed
- Removed `_raw` from QueryDSL

### Fixed
- Fixed `sort` query param during search

---
## [0.9.0] - 2019-02-11

### Added
- Embedded Xapian core 1.5.0 in the source code
- Accept application/x-ndjson as input for `:restore` command
- Home and docs site with cosmetic improvements

### Changed
- Greatly improves performance
- Documentation extended
- Upgraded to fmtlib v5.3.1 library
- Upgraded to ChaiScript v6.1.1 library
- Sort accepting casts
- QueryDSL with flattened queries

### Fixed
- Fixes a few issues with the way geospatial locations were
  being filtered and indexed

---
## [0.8.15] - 2019-02-01

### Added
- Keep-Alive and Close connection support

### Changed
- Compress documents using LZ4 instead of gzip
- Text fields no longer remove terms with Stopper

### Fixed
- Fix bug with UUIDs and macOS

---
## [0.8.14] - 2019-01-27

### Added
- Added new Xapiand client (based on Elasticsearch client)
- Added sorting for Aggregations
- Added `_keyed` to aggregations optional. Keyed are off by default now
- Added `_shift` to aggregations

### Deprecated
- Schema deprecated `string` type, it now only has `text` and
  `keyword` (`string` will be removed at some point)

---
## [0.8.13] - 2019-01-21

### Added
- Added bounds to standard deviation aggregation
- Documentation about Aggregations

### Changed
- Remote and Replication protocol are now split in two

### Fixed
- Range keys for aggregations fixed

---
## [0.8.12] - 2019-01-15

### Changed
- Documentation updated

### Fixed
- Fix Docker entrypoint

---
## [0.8.11] - 2019-01-14

### Changed
- Uses `/var/db/xapiand` directory for database by default

---
## [0.8.10] - 2019-01-10

### Changed
- Default bind address is now again 0.0.0.0

---
## [0.8.9] - 2019-01-10

### Fixed
- Fixes to multicast script for Kubernetes

---
## [0.8.8] - 2018-12-27

### Added
- Added daemon script for multicasting across k8s nodes

### Changed
- UDP usage tweaked (for Discovery)
- Default TCP bind address is now the current IP

---
## [0.8.7] - 2018-12-21

### Fixed
- Explicit initialization of schema for cluster database
- Cluster database endpoints normalized

---
## [0.8.6] - 2018-12-20

### Changed
- Using fnv1ah64 for clustering hash (`jump_consistent_hash`)

---
## [0.8.5] - 2018-12-20

### Changed
- Logo adjustments + banner updated

---
## [0.8.4] - 2018-12-16

### Fixed
- Fix Remote storage
- Discovery protocol fixed issues with cluster

---
## [0.8.3] - 2018-12-19

### Changed
- Discovery/Raft: Merged both into Discovery

### Fixed
- Colorize all node names
- Fix exit codes

---
## [0.8.2] - 2018-12-16

### Changed
- Improve efficiency of nodes update after PING

---
## [0.8.1] - 2018-12-13

### Changed
- Rewrite of Database Pool
- Thread names changed

---
## [0.8.0] - 2018-11-21

### Added
- Cluster mode enabled
- Adds replication and remote protocol

---
## [0.7.0] - 2018-09-21

### Added
- Prometheus metrics exposed at `/:metrics`

---
## [0.6.2] - 2018-05-03

### Added
- Added `--flush-threshold` command line option

### Changed
- `MERGE` and `PATCH` now require the document already exists
- Renamed command line option `--force-up` to simply `--force`

### Fixed
- Fix restore command memory usage
- Fix HTTP protocol on malformed messages
- Fix HTTP returning of large object bodies
- Fix indexing documents with integer IDs
- Fix io::read() to always return requested size when available
- Fix date accuracies

---
## [0.6.1] - 2018-04-25

### Added
- Parallel restore commands
- General optimizations

### Changed
- **BREAKING**: Geospatial fields changed generated terms. Reindexing needed!

### Fixed
- Fixed geospatial terms and accuracy
- Fixes storage (no longer returns 500)
- Fixes multithreading bug with collected statistics

---
## [0.6.0] - 2018-04-11

### Added
- Added :dump and :restore endpoints
- Added time it took (in milliseconds) to execute a :search query
- Added sort, limit and offset to query DSL
- Python: Xapiand client updated

### Changed
- **BREAKING**: Support for multi-content (by Content-Type) documents
- Using C++17
- Updated FreeBSD port

### Fixed
- GCC 7 compatibility
- Fixes problem with big body responses breaking the logs
- Towards MultiarchSpec compatibility for Linux

---
## [0.5.2] - 2018-04-02

### Changed
- Logo updated

### Fixed
- Fixes issues while compiling with GCC + cleanups

---
## [0.5.1] - 2018-03-28

### Added
- A Homebrew tap was created

### Changed
- Versioning of the project is now officially in the "0.x" line, after going
  through many ghost versions.
- Documentation was updated

---
## [0.5.0] - 2018-03-23

### Added
- Site is up and running
- Documentation is finally cooking in the oven.
- This release also features a new website (based on Jekyll's theme)

---
## [0.4.2] - 2018-02-16

### Fixed
- Fixes issues with "already locked database"

---
## [0.4.1] - 2017-10-02

### Changed
- First stable release
- Lots of optimizations

---
## [0.4.0] - 2016-11-14

### Fixed
- Fixed a few issues

---
## [0.3.0] - 2016-06-08

### Added
- First beta release

### Fixed
- Working under FreeBSD

---
## 0.0.0 - *2015-02-20*

- Birthday!

[Unreleased]: https://github.com/Kronuz/Xapiand/compare/v0.39.0...HEAD
[0.40.0]: https://github.com/Kronuz/Xapiand/compare/v0.39.0...v0.40.0
[0.39.0]: https://github.com/Kronuz/Xapiand/compare/v0.38.0...v0.39.0
[0.38.0]: https://github.com/Kronuz/Xapiand/compare/v0.37.0...v0.38.0
[0.37.0]: https://github.com/Kronuz/Xapiand/compare/v0.36.0...v0.37.0
[0.36.0]: https://github.com/Kronuz/Xapiand/compare/v0.35.0...v0.36.0
[0.35.0]: https://github.com/Kronuz/Xapiand/compare/v0.34.0...v0.35.0
[0.34.0]: https://github.com/Kronuz/Xapiand/compare/v0.33.0...v0.34.0
[0.33.0]: https://github.com/Kronuz/Xapiand/compare/v0.32.0...v0.33.0
[0.32.0]: https://github.com/Kronuz/Xapiand/compare/v0.31.0...v0.32.0
[0.31.0]: https://github.com/Kronuz/Xapiand/compare/v0.30.0...v0.31.0
[0.30.0]: https://github.com/Kronuz/Xapiand/compare/v0.29.0...v0.30.0
[0.29.0]: https://github.com/Kronuz/Xapiand/compare/v0.28.0...v0.29.0
[0.28.0]: https://github.com/Kronuz/Xapiand/compare/v0.27.0...v0.28.0
[0.27.0]: https://github.com/Kronuz/Xapiand/compare/v0.26.0...v0.27.0
[0.26.0]: https://github.com/Kronuz/Xapiand/compare/v0.25.0...v0.26.0
[0.25.0]: https://github.com/Kronuz/Xapiand/compare/v0.24.0...v0.25.0
[0.24.0]: https://github.com/Kronuz/Xapiand/compare/v0.23.0...v0.24.0
[0.23.0]: https://github.com/Kronuz/Xapiand/compare/v0.22.2...v0.23.0
[0.22.2]: https://github.com/Kronuz/Xapiand/compare/v0.22.1...v0.22.2
[0.22.1]: https://github.com/Kronuz/Xapiand/compare/v0.22.0...v0.22.1
[0.22.0]: https://github.com/Kronuz/Xapiand/compare/v0.21.2...v0.22.0
[0.21.2]: https://github.com/Kronuz/Xapiand/compare/v0.21.1...v0.21.2
[0.21.1]: https://github.com/Kronuz/Xapiand/compare/v0.21.0...v0.21.1
[0.21.0]: https://github.com/Kronuz/Xapiand/compare/v0.20.1...v0.21.0
[0.20.1]: https://github.com/Kronuz/Xapiand/compare/v0.20.0...v0.20.1
[0.20.0]: https://github.com/Kronuz/Xapiand/compare/v0.19.7...v0.20.0
[0.19.7]: https://github.com/Kronuz/Xapiand/compare/v0.19.6...v0.19.7
[0.19.6]: https://github.com/Kronuz/Xapiand/compare/v0.19.5...v0.19.6
[0.19.5]: https://github.com/Kronuz/Xapiand/compare/v0.19.4...v0.19.5
[0.19.4]: https://github.com/Kronuz/Xapiand/compare/v0.19.3...v0.19.4
[0.19.3]: https://github.com/Kronuz/Xapiand/compare/v0.19.2...v0.19.3
[0.19.2]: https://github.com/Kronuz/Xapiand/compare/v0.19.1...v0.19.2
[0.19.1]: https://github.com/Kronuz/Xapiand/compare/v0.19.0...v0.19.1
[0.19.0]: https://github.com/Kronuz/Xapiand/compare/v0.18.1...v0.19.0
[0.18.1]: https://github.com/Kronuz/Xapiand/compare/v0.18.0...v0.18.1
[0.18.0]: https://github.com/Kronuz/Xapiand/compare/v0.17.0...v0.18.0
[0.17.0]: https://github.com/Kronuz/Xapiand/compare/v0.16.1...v0.17.0
[0.16.1]: https://github.com/Kronuz/Xapiand/compare/v0.16.0...v0.16.1
[0.16.0]: https://github.com/Kronuz/Xapiand/compare/v0.15.0...v0.16.0
[0.15.0]: https://github.com/Kronuz/Xapiand/compare/v0.14.0...v0.15.0
[0.14.0]: https://github.com/Kronuz/Xapiand/compare/v0.12.2...v0.14.0
[0.12.2]: https://github.com/Kronuz/Xapiand/compare/v0.12.1...v0.12.2
[0.12.1]: https://github.com/Kronuz/Xapiand/compare/v0.12.0...v0.12.1
[0.12.0]: https://github.com/Kronuz/Xapiand/compare/v0.11.1...v0.12.0
[0.11.1]: https://github.com/Kronuz/Xapiand/compare/v0.11.0...v0.11.1
[0.11.0]: https://github.com/Kronuz/Xapiand/compare/v0.10.2...v0.11.0
[0.10.2]: https://github.com/Kronuz/Xapiand/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/Kronuz/Xapiand/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/Kronuz/Xapiand/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/Kronuz/Xapiand/compare/v0.8.15...v0.9.0
[0.8.15]: https://github.com/Kronuz/Xapiand/compare/v0.8.14...v0.8.15
[0.8.14]: https://github.com/Kronuz/Xapiand/compare/v0.8.13...v0.8.14
[0.8.13]: https://github.com/Kronuz/Xapiand/compare/v0.8.12...v0.8.13
[0.8.12]: https://github.com/Kronuz/Xapiand/compare/v0.8.11...v0.8.12
[0.8.11]: https://github.com/Kronuz/Xapiand/compare/v0.8.10...v0.8.11
[0.8.10]: https://github.com/Kronuz/Xapiand/compare/v0.8.9...v0.8.10
[0.8.9]: https://github.com/Kronuz/Xapiand/compare/v0.8.8...v0.8.9
[0.8.8]: https://github.com/Kronuz/Xapiand/compare/v0.8.7...v0.8.8
[0.8.7]: https://github.com/Kronuz/Xapiand/compare/v0.8.6...v0.8.7
[0.8.6]: https://github.com/Kronuz/Xapiand/compare/v0.8.5...v0.8.6
[0.8.5]: https://github.com/Kronuz/Xapiand/compare/v0.8.4...v0.8.5
[0.8.4]: https://github.com/Kronuz/Xapiand/compare/v0.8.3...v0.8.4
[0.8.3]: https://github.com/Kronuz/Xapiand/compare/v0.8.2...v0.8.3
[0.8.2]: https://github.com/Kronuz/Xapiand/compare/v0.8.1...v0.8.2
[0.8.1]: https://github.com/Kronuz/Xapiand/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/Kronuz/Xapiand/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/Kronuz/Xapiand/compare/v0.6.2...v0.7.0
[0.6.2]: https://github.com/Kronuz/Xapiand/compare/v0.6.1...v0.6.2
[0.6.1]: https://github.com/Kronuz/Xapiand/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/Kronuz/Xapiand/compare/v0.5.2...v0.6.0
[0.5.2]: https://github.com/Kronuz/Xapiand/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/Kronuz/Xapiand/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/Kronuz/Xapiand/compare/v0.4.2...v0.5.0
[0.4.2]: https://github.com/Kronuz/Xapiand/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/Kronuz/Xapiand/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/Kronuz/Xapiand/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/Kronuz/Xapiand/compare/v0.0.0...v0.3.0
