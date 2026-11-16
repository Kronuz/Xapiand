---
title: "Architecture Overview"
---

Xapiand is one process that serves a REST API over HTTP and keeps its indexes on
local disk. Requests run as tasks on one event loop per processor; each shard of an index is
a [strata](https://github.com/Kronuz/strata) segment store searched through
[prospect](https://github.com/Kronuz/prospect), both linked in as libraries; and a
write-ahead log per shard covers what the last commit does not hold. This page
explains how those pieces fit and what happens to a write and to a search.

Two companion pages go deeper:

- [Dependencies](/Xapiand/architecture/dependencies): every library the build
  fetches, by role, and how the composite ones depend on each other.
- [Internals](/Xapiand/architecture/internals): the source layout, the conventions
  and the invariants a change must keep.

## The process

```d2 alt="Xapiand's process: clients reach the HTTP loops; their requests reach the index registry, which holds the strata indexes; device work runs on the work pool, log lines go to the WAL writers, and the control loop commits and stops the node"
direction: right
Clients: "Clients\n(JSON, YAML, MessagePack)"
HTTP: "HTTP loops\n(one per processor)"
Registry: "Index registry\n(pins, turns, views)"
Engine: "strata + prospect\n(one index per shard)"
Pool: "WorkPool\n(device work)"
WAL: "WAL writers"
Control: "Control loop\n(autocommit, signals, stop)"
Clients -> HTTP
HTTP -> Registry: "tasks"
Registry -> Engine
Engine -> Pool: "chores"
Registry -> WAL: "log lines"
Control -> Registry: "commits"
```

Each box is a set of threads with one job:

| Threads | How many | What runs there |
|---|---|---|
| Control loop | 1, the main thread | Node setup, signals, the minute housekeeping, the debounced autocommit, the stop |
| HTTP loops | one per processor (`--processors`, all of the machine's by default) | Connections, request parsing, the views (the request handlers) as tasks, query parsing, document preparation |
| WorkPool | one per hardware thread | Chores: opens, refreshes, commits, closes, blob reads and writes, large-body decoding, directory walks, merges |
| WAL writers | half the processors, 1 to 10 (`--writers`) | Appending log lines |
| fsync threads | half the processors, 1 to 10 | The debounced fsync of the log |

The rule that holds it together: **a loop never blocks**. A task that has to wait
awaits a timer, an event, an index's turn or a chore. Anything that touches a
device runs as a chore on the WorkPool and resumes the task on its own loop when
it finishes. Debug builds abort on a blocking call inside a loop, and a CI step
fails on any blocking primitive its allowlist does not name.

All HTTP loops serve one port. On Linux each loop has its own socket with
`SO_REUSEPORT`, so the kernel balances the accepts; elsewhere one acceptor hands
the connections out. The listeners open once the node reaches the ready state.

## A request

A connection is a coroutine on the loop that accepted it, and each request on it
is a task on that same loop:

1. **Admission.** A loop admits at most 1,000 requests at once, and refuses new
   ones while 256 of its tasks wait for their next slice. Both answer
   `503 Service Unavailable` with `Retry-After: 1` before the body is read.
2. **The body.** A body is buffered up to 16 MiB (`413` past it); one past 64 KiB
   is decoded in a chore, so a large document does not hold the loop. A `RESTORE`
   body streams: each batch is parsed in a chore while the batch before it is
   indexed. A blob (a stored file that is not JSON, YAML or MessagePack) too large
   to buffer streams to disk in chores, up to 4 GiB.
3. **Routing.** The method and the URL's shape (an index, a document id, a
   `:command`) select a view from one route table.
4. **Slices.** A view runs in slices of 200 µs. A search yields its loop between
   windows of postings and a restore between documents, so one heavy request
   cannot starve the others on its loop.
5. **Cancellation.** A client that disconnects, or a node that stops, ends a read
   at its next checkpoint. An admitted write always completes.

## Indexes on disk

An index is a directory. Its shards are subdirectories, and each shard holds a
strata index, the shard's write-ahead log and its blob volumes:

```text
<data>/
├── .xapiand/
│   ├── node              the node's name
│   └── indexes/          every index's settings and, by default, its schema
└── twitter/              an index with the default 5 shards
    ├── .__1/
    │   ├── strata/       segments, live-docs files, manifests
    │   ├── wal.*         the write-ahead log
    │   └── docdata.*     blob volumes
    ├── .__2/
    └── …
```

An index created with one shard keeps those files in its own directory. Indexes
nest: `twitter/user` is an index inside `twitter`'s directory, and the pattern
`twitter/**` names both.

A document belongs to one shard. Its id goes to one of 1,024 fixed buckets (a
numeric id `n` to bucket `(n - 1) mod 1024`, any other id to the FNV-1a hash of
the id modulo 1,024), and bucket `b` to shard `b mod shards`. A new document
without an id goes to the shard that holds the fewest documents, which issues
its numeric id from a bucket it owns. Docids are each shard's own, in append
order, and never derive from an id.

The **index registry** keeps the open indexes. It is a strata catalog keyed by
path: a request *pins* an index to use it, and the first pin opens it in a chore.
Writes to an index take its **turn**, an asynchronous mutex with a 30 s deadline
(`503` when the turn does not come). Reads take no turn: each holds a **view**,
the index as its last refresh published it, so a read never makes a write wait.
An index idle for 30 s gives back its memory maps, and one idle for 10 minutes
closes; when more are open than `--database-pool-size` (200, lowered to fit the
process's file-descriptor limit) it closes the least used first.

## The write path

```text
the view (HTTP loop)          the index's turn               later
────────────────────          ────────────────               ─────
schema -> engine document     strata applies it              autocommit: 1 s after the
pick the shard                  (docid, version, key)          last write, at most 8-10 s
blobs -> docdata (chore)      log line queued                commit: segments, manifest,
                              turn released                    fsync; revision + 1
```

1. **Prepare.** The view resolves the index's settings and schema (both cached,
   and read once however many requests ask for them at once). The schema turns the
   document into the engine's form: terms with positions, values by slot, the
   stored record (the document as MessagePack) and its key, the document id.
2. **Apply.** Under the index's turn strata checks the expected version, assigns
   the docid and the new version, and adds the document to its write buffer. The
   log line is queued in the same turn, so the log's order is the apply's order.
3. **Answer.** The write is acknowledged once it is applied and its log line is
   queued. With `?commit` the response waits for a commit; otherwise the write
   schedules the autocommit, which commits the index 1 s after its last write,
   3 s after each write while writes keep coming, and never later than 8 to 10 s
   after the first uncommitted one.
4. **Commit.** A commit syncs the blob volumes, then strata writes the buffered
   documents as segments and a new manifest and fsyncs them. The index's revision
   goes up by one and the log gets a commit line. Commits of one index coalesce:
   a writer whose writes a running commit already covers waits for that one.

**Visibility.** A read refreshes the index first when anything was applied since
the last refresh, so a read sees every acknowledged write, committed or not.

**Durability.** A write is durable once its index commits. Before that it is in
strata's buffer and in the log: a WAL writer appends the line, and the fsync that
follows is debounced to within a few seconds. A crash or a power loss can lose
the latest acknowledged writes that were not committed. Opening an index replays
the log lines its last commit does not hold and commits them. Replay is
idempotent: each line carries the version it wrote and applies only as the next
version of its document. A log that does not read back is quarantined, not
deleted.

`RESTORE` writes no log: it indexes in batches and commits at its end, so a
restore that a crash interrupts is run again.

## The search path

1. **Parse.** A query is the DSL in the body (`_query`) or a query string (`q=`,
   which [boolean-parser](https://github.com/Kronuz/boolean-parser) reads into the
   same DSL). A text field's query goes through
   [queryparser](https://github.com/Kronuz/queryparser) with the field's language,
   stemmer and stop words. The result is one engine query tree, refused past 100
   levels or 4,096 nodes.
2. **Name the indexes.** Names, lists and patterns (`*`, `**`, `_all`, `-name`)
   expand against the data directory in a chore, and each index resolves to its
   shards.
3. **Search in batches.** A search over many indexes holds one batch open at a
   time (`--concurrent-search-indexes`, by default a loop's share of the pool),
   merges each batch's best hits into the top of the whole, and releases it. A
   search that names more than `--max-search-indexes` (100,000) shards is refused
   with `400`.
4. **Score and collect.** For each shard prospect walks the postings of the view
   the search holds, scores with BM25, keeps the top hits (or the first by a sort
   key, collapsed by a field when asked), and hands every match to the
   aggregations.
5. **Answer.** The hits' stored records are read and returned with the totals and
   the aggregations.

## Stopping

`SIGTERM` or `SIGINT` stops the node. It waits for the requests in flight, then
stops in a fixed order within one budget of 35 s: the HTTP service stops and lets
admitted writes finish, the waiting autocommits are dropped (closing an index
commits what it has pending), every index commits and closes, and the logs and
the fsync threads finish. A stop that does not end within the budget logs what
was still running and exits with status 75; the next start replays the logs.
[Stopping the node](/Xapiand/options#stopping-the-node) gives the details and the
exit statuses.

## Limits

| What | Limit | Past it |
|---|---|---|
| Requests in flight, per HTTP loop | 1,000 | `503`, `Retry-After: 1` |
| Tasks waiting for a slice, per HTTP loop | 256 | `503`, `Retry-After: 1` |
| Buffered body | 16 MiB | `413` |
| Streamed blob | 4 GiB | `413` |
| Nesting of a JSON, YAML or MessagePack body | 200 levels | `400` |
| Query | 100 levels, 4,096 nodes | `400` |
| Aggregations | 16 levels, 65,535 buckets | `400` |
| Shards one search names | 100,000 (`--max-search-indexes`) | `400` |
| Wait for an index's turn | 30 s | `503` |
| Views held on one index at once | 16 per HTTP loop, plus 2 per processor, plus 2 | `503` |
| Open indexes | `--database-pool-size` (200) | the least used close |
| Blob volume | 32 GiB | the next volume |
| One Lua script call | 10⁸ instructions, 64 MiB of memory, a 16 MiB result | `400`, or `413` for the result |
| The stop, after the requests in flight | 35 s | exit status 75 |

## One node

This release runs one node: there is no cluster, replication or node discovery.
An index's `number_of_replicas` setting is stored and has no effect. Distribution
is planned on strata's replication groups, one per shard
([DISTRIBUTION.md](https://github.com/Kronuz/strata/blob/main/docs/DISTRIBUTION.md)).

## Where to go next

- **Build it:** [Building from Sources](/Xapiand/building): requirements, the
  build and its CMake options.
- **Run it:** [Options](/Xapiand/options): the command-line options and the stop.
- **The libraries:** [Dependencies](/Xapiand/architecture/dependencies).
- **The code:** [Internals](/Xapiand/architecture/internals).
- **The engine:** strata's
  [ARCHITECTURE.md](https://github.com/Kronuz/strata/blob/main/docs/ARCHITECTURE.md)
  and prospect's
  [ARCHITECTURE.md](https://github.com/Kronuz/prospect/blob/main/docs/ARCHITECTURE.md).
