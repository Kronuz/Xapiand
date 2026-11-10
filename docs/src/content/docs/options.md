---
title: "Options"
---

`xapiand --help` lists every option of the node, with its type and its allowed
values. An option takes its value as `--port 8880` or `--port=8880`, and an
option that does not exist is an error: the node prints `The following
arguments were not expected` and exits.

## Verbosity

Verbosity of the servers logs can be set by using the `-v`, `--verbose` or
`--verbosity` options (`-v` can be repeated, and adds to `--verbosity`).
_*very-very-very* verbose_ output is usually enabled with `-vvvv` or
`--verbosity=4`. This mode also enables `--human`, `--echo`, `--pretty` and
`--comments` options by default.

Each of these four options has a `--no-` form that turns it off even at verbosity 4
(`--no-echo`, `--no-pretty`, `--no-human`, `--no-comments`), and each can be set
for a single request with the query parameter of the same name: `?echo`,
`?pretty`, `?human` and `?comments` turn it on, `?echo=false` and so on turn it
off. The request wins over the node.

### Echo

Echo makes Xapiand return newly created or edited objects as part of the
response (`200 OK` with the document, its `_id` and its `_version`). Without it,
a write with an ID in the URL answers `204 No Content` and a `POST` without an ID
answers `201 Created` with the document's path in the `Location` header, both
without a body (see [What a write answers](/Xapiand/quickstart#what-a-write-answers)).
Returning a body can be enabled with the `--echo` option, with `?echo` on the
request, or by using a verbosity level higher or equal to 4.

### Pretty

Pretty makes Xapiand return pretty (formatted) JSON output as responses. This
option can be enabled with the `--pretty` option, with `?pretty` on the request,
or by using a verbosity level higher or equal to 4.

```rest
PUT /options/demo/1

{
  "title": "Options"
}
```

```rest
GET /options/demo/1?pretty=true
```

<!-- e2e:begin
---
description: A pretty response is indented
---

```js
pm.test("The response is indented", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.text()).to.include('\n  "title": "Options"');
});
```
e2e:end -->

```rest
GET /options/demo/1?pretty=false
```

<!-- e2e:begin
---
description: A response with pretty off is one line
---

```js
pm.test("The response is one line", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.text().trim()).to.not.include('\n');
  pm.expect(pm.response.json().title).to.equal("Options");
});
```
e2e:end -->

### Human

Human makes Xapiand return humanized numbers for size and times in the output
of the responses (`"took": "39.889ms"` instead of seconds as a number). This
option can be enabled with the `--human` option, with `?human` on the request, by
setting output to be "pretty" or by using a verbosity level higher or equal to 4.

```rest
SEARCH /options/demo/?human=true
```

<!-- e2e:begin
---
description: A humanized search takes a string
---

```js
pm.test("took is humanized", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().took).to.be.a('string');
});
```
e2e:end -->

```rest
SEARCH /options/demo/?human=false
```

<!-- e2e:begin
---
description: A search without human takes seconds
---

```js
pm.test("took is a number of seconds", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().took).to.be.a('number');
});
```
e2e:end -->

### Comments

Comments add the fields that start with `#` to the documents of a response:
`#docid` and `#shard` to a document, and `#rank`, `#weight` and `#percent` to the
hits of a search. They can be enabled with the `--comments` option, with
`?comments` on the request, or by using a verbosity level higher or equal to 4.

```rest
GET /options/demo/1?comments=true
```

<!-- e2e:begin
---
description: A document with comments
---

```js
pm.test("The document carries its comments", function () {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData).to.have.property("#docid");
  pm.expect(jsonData).to.have.property("#shard");
});
```
e2e:end -->

### Console

- `--color` sets when the console output is colored: `auto` (the default),
  `always`, `never`, `truecolor`, `256` or `16` (`on`, `off`, `24bit`,
  `256color`, `ansi` and `stacked` are accepted too). `--no-color` is the same
  as `--color=never`, and an explicit `--color` wins.
- `--iterm2` sets marks, tabs, title, badges and growl notifications in iTerm2.
- `--log` takes one or more of `epoch`, `iso8601`, `timeless`, `seconds`,
  `milliseconds`, `microseconds`, `thread-names` and `locations`, which change
  the time stamp of each log line and add the thread name and the source location.
- `-L`, `--logfile` writes the log to a file.

## Strict

Schemas are normally automatically created by default, guessing the type of new
fields being indexed. The `--strict` option disables this
[Dynamic Typing](/Xapiand/reference-guide/schemas/dynamic-typing)
feature and forces the user to specify a type for all new fields, making the
request fail with `412 Precondition Failed` if the datatype is missing.

The same switch is a property of one index's schema, `_strict`, which a node
without `--strict` honors for that index alone:

```rest
PUT /options/strict/

{
  "_schema": {
    "_strict": true,
    "name": {
      "_type": "keyword"
    }
  }
}
```

```rest
PUT /options/strict/1

{
  "name": "typed",
  "nickname": "untyped"
}
```

<!-- e2e:begin
---
description: A strict index refuses a field without a type
---

```js
pm.test("The field without a type is refused", function () {
  pm.response.to.have.status(412);
  pm.expect(pm.response.json().message).to.include("nickname");
});
```
e2e:end -->

## Node and Data

- `-D`, `--database` sets the path to the root of the node, where all index data
  lives (`<prefix>/var/db/xapiand` by default, where `<prefix>` is the install
  prefix). The node refuses a directory that holds data from a Xapian-based
  release and exits with status 65; see [Upgrading](/Xapiand/upgrade).
- `--force` uses the path as the root of the node without checking that its
  `.xapiand` directory is empty or holds only the node's own files; the node exits
  with status 65 when that check fails.
- `--name` sets the node name.
- `--shards` sets the default number of shards per index (5).
- `--port` sets the HTTP REST API port (8880).
- `--bind-address` sets the address the node listens on (all addresses by default).
- `-d`, `--detach` runs the process in the background. It writes the log to
  `<prefix>/var/log/xapiand.log` and the process ID to `<prefix>/var/run/xapiand.pid`
  unless `-L`, `--logfile` and `-P`, `--pidfile` say otherwise.
- `-P`, `--pidfile` saves the process ID in a file.
- `--uid` and `--gid` set the user and the group the node runs as. A node started
  as root needs `--uid`.
- `--processors` sets how many processors the node uses (all by default). It runs one HTTP loop per processor,
  and sizes the fsync threads (half the processors, at least 1 and at most 10).

## Identifiers

- `--uuid` selects how generated document IDs and UUIDs are written: `vanilla` (the
  standard text form), `encoded` (base59) or, on builds that enable them, `guid`
  and `urn`; `compact` shortens the form, and `partition` partitions UUID index
  paths. Without it the node uses `encoded` and `compact`.
- `--legacy-ids` mints and decodes UUIDs with the legacy v1 codec only (pure v1,
  no v6), for a deployment that must stay byte-for-byte compatible with pre-1.0 ids.

## Administrative Commands

`--admin-commands` enables the `FLUSH` and `QUIT` commands, which a node answers
only at the root URL (`FLUSH /`). `FLUSH` commits and closes every open index and
answers when they are closed; `QUIT` asks the node to shut down (see
[Stopping the node](#stopping-the-node)).
Without the option both answer `405 Method Not Allowed`, as on this node:

```rest
FLUSH /
```

<!-- e2e:begin
---
description: FLUSH needs --admin-commands
---

```js
pm.test("FLUSH is not allowed without --admin-commands", function () {
  pm.response.to.have.status(405);
});
```
e2e:end -->

The API has no authentication, so enable the option only on a node that
untrusted clients cannot reach.

## Capacity and Caches

- `--max-clients` is the most client connections the node holds open (1000).
- `--max-files` is the most files the node opens. By default the node computes
  what its clients and indexes need, and raises the process limit to it; it
  lowers `--max-clients` and `--database-pool-size` when the limit is too low.
- `--database-pool-size` is the most indexes (shards) the pool keeps open (200).
- `--schema-pool-size` is the most schemas the node keeps (100), and
  `--schema-versions-size` the most versions of schema it keeps in its cache (100).
- `--scripts-cache-size` is the cache size for compiled scripts (100).
- `--resolver-cache-size` is the cache size for resolved index paths (100).
- `--wal-writer-cache-size` is the cache size of the write-ahead log writers (100).

## Searching many indexes

A search that names several indexes (a list, a wildcard, `_all`; see [Multiple Indexes](/Xapiand/reference-guide/api#multiple-indexes))
reads them in batches: it holds at most one batch of indexes open at once, searches it, merges its best hits into the
result and moves on to the next. A search over thousands of indexes therefore needs only a batch's worth of open
indexes, however wide it is. A batch holds whole indexes (all the shards of an index are read together), so an index
with more shards than the batch is a batch of its own.

- `--concurrent-search-indexes` sets the batch size: the most shards one search holds open at once. By default it is a
  loop's share of the open indexes the database pool holds (`--database-pool-size` over the HTTP loops, at least 1), so
  one wide search on every loop at once holds about what the pool does.
- `--search-walk-queue` (8, at least 0) and `--search-walk-wait` (10 seconds) bound the lookup of index names for
  patterns: the node reads its data directory for one search at a time, a search that finds `--search-walk-queue` others
  waiting answers `429`, and one that waits `--search-walk-wait` seconds for its turn answers `503`; both carry
  `Retry-After: 1`. Searches by name, writes and everything else are not held by it.
- `--max-search-indexes` (100,000, at least 1) is a cap on a runaway pattern. A search that names more indexes, counting
  the ones a wildcard expands to, answers `400` with the limit in the message. So does one whose patterns read more
  than that many directory entries to find their indexes, whether the entries match or not (`*,-*` matches nothing and
  reads the whole tree, and the directories of nested indexes count as well): the same number bounds the names and the work. A pattern goes only into the directories it can still match below (`logs/2026/*` reads `logs` and `logs/2026`), a `**` goes everywhere. A data directory with more entries in one
  directory than the cap cannot be walked by a wildcard; name the indexes, or raise the cap. Matching the names has a budget too
  (4,096 steps per entry the cap allows, so patterns that backtrack over long names answer `400` rather than hold a
  thread). A search names at most 32 patterns (wildcards and `-` exclusions together).

Each index is read as its batch starts, so a search does not see all its indexes at one moment; each index is whole, and
a write acknowledged before the search began is in it. A relevance weight is read against the statistics of the index
the document is in (its document count, its term frequencies), as Elasticsearch reads a shard's: a document weighs the
same in a search of one index and in a search of a thousand, and the hits, their order, their weights, the totals and
the aggregations do not depend on the batch size. The `xapiand_search_pinned_indexes_peak` metric reports the most
indexes searches have held open at once.

## Write-Ahead Log

- `--writers` sets the number of WAL writer threads (half the processors, at least 1 and at most 10).
- `--wal-writer-cache-size` sets the size of the writers' cache (100). Both tune the
  [WAL](/Xapiand/reference-guide/wal) writer pool.

## Dump and Restore

`--dump INDEX` writes the documents of an index and `--restore INDEX` reads them
into one, without starting the HTTP server: the node uses the data directory
(`-D`) alone. They read and write MessagePack, on stdout and stdin unless
`-o`, `--out` (with `--dump`) or `-i`, `--in` (with `--restore`) name a file,
and they log the SHA-256 of the stream. `--dump` and `--restore` cannot be used
together, and `--shards` sets the shard count of the restored index. The
`DUMP` and `RESTORE` requests do the same over HTTP; see
[Backups](/Xapiand/reference-guide/indexes/backups).

```sh
xapiand -D /var/db/xapiand --dump twitter/user --out users.mp
xapiand -D /var/db/new --restore twitter/user --in users.mp
```

## Monitoring

`GET /:metrics` answers the node's metrics in the Prometheus text format, as
`xapiand_*` series:

```rest
GET /:metrics
```

<!-- e2e:begin
---
description: The Prometheus metrics endpoint
---

```js
pm.test("The metrics are in the Prometheus text format", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.headers.get("Content-Type")).to.include("text/plain");
  pm.expect(pm.response.text()).to.include("# TYPE xapiand_running gauge");
});
```
e2e:end -->

`xapiand_reactor_lag_seconds` reports how late each event loop runs. The probes
behind it wake every 1 ms while a loop serves requests, and `XAPIAND_LAG_PROBE=0`
in the environment starts the node without them.

## Stopping the node

`SIGINT` or `SIGTERM` stops the node. The stop has two waits, and the node exits when the second ends:

1. **The clients.** The node waits for the requests in flight. With clients that keep sending, this has taken up to about 45 s.
2. **The rest, at most 35 s in all.** In this order, each step starting when the one before it has ended:
   1. The HTTP service stops: it stops accepting, a read ends at its next checkpoint, an admitted write is let finish (one that waits for its index's turn answers `503` after 30 s), the connections of what is left are cut after a grace of 15 s (a write that finishes after that applies, but its client is not answered), and the loops stop one grace later.
   2. The autocommits that were waiting are dropped; closing an index commits what it has pending.
   3. Every index closes.
   4. The write-ahead logs flush.
   5. The fsync threads finish.

What is still running when the 35 s end (steps 1 to 3 share them) is logged and abandoned: the requests in flight per loop, and the indexes whose turn or references are held. The node then flushes the
write-ahead logs and exits. Without abandoned work the status is 0; with it the status is 75 (`EX_TEMPFAIL`) and the log lists what was abandoned. An index that cannot close within the 35 s (for example on a hung
disk) counts, whether or not a request is in flight: the node logs `Shutdown abandoned the indexes that are still in use` and exits 75. The next start replays the log. A write answered with a 2xx
is in the log; one that was not answered was never promised.

A second `SIGINT` or `SIGTERM` during the second wait ends it at once: the node logs what is unfinished, flushes the logs and exits 75. The longest stop is therefore the first wait plus 35 s, plus the
flush of the logs; set a supervisor's stop timeout above that (for example 120 s). A node that cannot set itself up (a port that is taken) logs why and exits with status 70.

## HTTPS

- `--tls-cert` and `--tls-key` (PEM files, given together) serve the REST API over TLS on
  `--port`, in place of plain HTTP. The key file must be readable by its owner only (mode
  `0600`); the certificate must be valid now, and Xapiand warns when it expires within 14
  days. TLS 1.2 and 1.3 only, forward-secret ciphers only, no session resumption.
- `--tls-max-connections` caps the TLS connections held at once (1024 by default; it must
  fit the open-files limit). Over TLS, a connection idle 75 seconds, headers not done in 30
  seconds, or a request or reply that moves no data for 60 seconds is closed.

HTTPS protects traffic between clients and the node. It does not authenticate clients:
anyone who can reach the port can use the API.

## Environment

- `XAPIAND_DATA_PATH` is the directory that holds the node's `stopwords/` and
  `mime.types`. Without it the node looks beside the executable (`../share/xapiand`,
  or the source tree for a build that is not installed) and then in the install
  prefix.
- `XAPIAND_LAG_PROBE=0` starts the node without the event-loop lag probes (see
  [Monitoring](#monitoring)).
- The Docker image reads `XAPIAND_DATABASE`, `XAPIAND_NAME`, `XAPIAND_UID` and
  `XAPIAND_GID` (see [Installation](/Xapiand/installation#docker)).

## Removed Options

These options do not exist any more, and the node rejects them as unknown
arguments. The cluster went away with its options, until it returns, and the
thread counts went away with the pools they sized.

- Cluster: `--solo`, `--cluster`, `--discovery-port`, `--discovery-group`,
  `--discovery-interface`, `--xapian-port`, `--replica-port`, `--primary-node`,
  `--matchers`, `--flush-threshold`, `--replicas`, `--replicators`,
  `--remote-clients`, `--remote-servers`, `--replication-clients`,
  `--replication-servers`, `--discoverers` and `--database-stall-time`.
- Threads: `--committers`, `--fsynchers`, `--http-servers`, `--http-clients`,
  `--bulk-preparers` and `--bulk-indexers`. Use `--processors`.
- `--max-database-readers`: an index is opened once and shared by every request.

## Build-Conditional Options

A build configured with `-DRANDOM_ERRORS=ON` adds `--random-errors-net`,
`--random-errors-io` and `--random-errors-db`, which inject errors with the
probability (0 to 1) they are given. A build with `-DENGINE_CENSUS=ON` writes the
engine facilities each request uses to the file `XAPIAND_ENGINE_CENSUS` names,
and one with `-DCRASH_POINTS=ON` kills the process at the commit or replay point
`XAPIAND_STRATA_CRASH=point:n` names. They are for testing; see
[Building](/Xapiand/building#build-options).

For the complete, authoritative list, run `xapiand --help`.
