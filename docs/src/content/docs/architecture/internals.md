---
title: "Internals"
---

This page is for contributors: where the code is, the conventions it follows
everywhere, and the invariants that look like implementation details but hold
the system up. For the runtime picture, start with the
[Architecture Overview](/Xapiand/architecture); for the libraries, see
[Dependencies](/Xapiand/architecture/dependencies). The repository's
[AGENTS.md](https://github.com/Kronuz/Xapiand/blob/master/AGENTS.md) is the full
working guide.

## Source layout

`src/` is Xapiand's own code. Everything else, the search engine included, is a
standalone library the build fetches (see
[Dependencies](/Xapiand/architecture/dependencies)); a bug in a library is fixed
in that library's repository. The entry point is `src/main.cc`, and
`XapiandManager` (`src/manager.*`) owns the node: the control loop, the HTTP
service and the stop.

| You're touching… | Start in |
|---|---|
| The HTTP API: routes and request handlers | `src/server/search_views.cc` (the route table and the views), `src/server/search_service.*`, `src/server/http_utils.h`, `src/server/mime_types.*` |
| Index paths, patterns and settings | `src/endpoint.*`, `src/index_path.h`, `src/index_pattern.*`, `src/index_resolver_lru.*` |
| The control loop, autocommit and the stop | `src/manager.*`, `src/control_plane.h`, `src/shutdown_wait.h` |
| The index registry: pins, turns, chores | `src/database/pool.*` |
| Writes, commits and blob storage of a shard | `src/database/shard.*`, `src/database/data.*` |
| The write-ahead log | `src/database/wal.*` |
| Documents in and out, search, restore | `src/database/handler.*` |
| Schemas and field types | `src/database/schema.*`, `src/database/schemas_lru.*`, `src/reserved/` |
| The engine seam | `src/engine/` (the interface), `src/engine/strata/` (strata and prospect behind it) |
| Query languages | `src/query_dsl.*` (the DSL; the `q=` string through boolean-parser), `src/engine/text.*` (text fields through queryparser), `src/engine/query.*` (the query tree) |
| Aggregations | `src/aggregations/` |
| Sorting, ranges and geospatial terms | `src/multivalue/` |
| Geospatial shapes | `src/geospatial/` (the shapes; the mesh is the htm library) |
| Value encoding | `src/serialise*.{cc,h}`, `src/cast.*`, `src/length.*` |
| Lua scripts | `src/script.*`, `src/lua/` |
| Metrics | `src/metrics.*`, `XapiandManager::server_metrics_impl` |
| Command-line options | `src/opts.*` |
| Logging | `src/log.h`, `src/logger_fwd.h` (the engine is the logger library) |
| HTTPS | `src/tls.*` |

Tests: C++ unit and fuzz targets in `tests/`, end-to-end scripts in `harness/`
(`harness/run_all.sh` runs the suites), and the docs E2E, which runs every request
example on this site against a node.

## The engine seam

Xapiand reaches the engine through `src/engine/`: a shard interface
(`engine::Shard`: writes, commits, transactions, reads, refresh, close), the
document in the engine's own form (`engine::Document`), the query tree
(`engine::Query`) and search (`engine::search`), and the two ways a host runs
work (`engine::Host`, `engine::Chores`). `src/engine/strata/` implements it over a
prospect index. A document is built in strata's form from the start: the schema
calls the document's `add_term`, `add_value` and `set_data`, the shard applies
that object, and the log serialises the same one, so nothing is converted on the
way.

The index registry and its entries are the one place outside `src/engine/` that
names strata and prospect: `DatabasePool` is a strata catalog of prospect indexes
(`src/database/pool.*`) and `ShardEndpoint` opens one (`src/database/shard.cc`),
since opening, pinning and closing an index is the catalog's job.

A build with `-DENGINE_CENSUS=ON` (off by default) counts the engine facilities
each request uses at the seam and writes them to the file
`XAPIAND_ENGINE_CENSUS` names (`src/engine/census.*`).

## Conventions

**Never block a loop.** A request, and the node's own work, is a task on an
event loop. A task that has to wait awaits a timer, an event, an index's turn or
a chore (device work on the `WorkPool`); it never calls `sync_wait`, waits on a
condition variable, sleeps or makes a file call. `engine::check_may_block` aborts
a debug build that does, and `harness/async_check.sh --strict` (a CI step) fails
on any blocking primitive that `harness/async_allowlist.tsv` does not classify.

**Chores have a class.** `query` is work a request waits for (a refresh, an open,
a blob read), `flush` is throughput work (a commit, a close, a blob write), and
`merge` is background work (`src/engine/chores.h`). Each index runs its chores in
a work group of its own on the one `WorkPool` (`IndexChores`,
`src/database/pool.h`).

**MsgPack is the universal value.** JSON, YAML, MessagePack and internal objects
are all `MsgPack`, the copy-on-write value of the
[msgpack](https://github.com/Kronuz/msgpack) library: copies share one
reference-counted buffer and copy only on mutation. Reserved keys start with `_`
(`src/reserved/`).

**Perfect-hash dispatch.** Reserved words (the query DSL, aggregations, field
types, casts) dispatch through compile-time perfect-hash tables
(`phf::make_phf`, the [constexpr-phf](https://github.com/Kronuz/constexpr-phf)
library) and a `switch`, not if-ladders. Adding a keyword extends both.

**Logging macros `L_*`.** Most category macros (`L_CALL`, `L_DATABASE`, …)
compile to nothing; switch one on by editing its `#define` near the top of
`src/log.h` (or of the file you are tracing) and rebuild. Arguments are evaluated
lazily, so an expensive one costs nothing when the message is filtered out.

**Order-preserving values.** A value stored for sorting and ranges is encoded so
that comparing its bytes compares the values. Change an encoding and you change
what sorts and ranges return, so any new encoding keeps that order.

## Load-bearing invariants

These look safe to "clean up" and are not:

- **A task holds one index's turn at most, and takes no turn, open or drain while
  it holds a view.** Either would let two tasks wait for each other. Debug builds
  count both per task and abort on a breach (`src/engine/host.h`).
- **The turn covers the apply, the revision and the log line.** The log line is
  queued under the same turn as the apply, so the log's order is the apply's
  order (`Turn`, `src/database/pool.h`). Don't queue a log line outside it.
- **Replay is idempotent.** Each log line carries the version its document was
  written at, and replay applies it only as the next version of that document; a
  line further ahead means the log lost a line and fails the open
  (`replay_applies`, `src/engine/strata/shard.cc`). Don't add a log operation that
  cannot be applied twice.
- **A read never takes the turn to read.** It takes the turn only to refresh an
  index that has unrefreshed writes, then reads a view. A read that held the turn
  would make every write to its index wait for it.
- **Trixel ids encode the mesh path, 2 bits per level, big-endian.** That is why a
  region on the sphere maps to contiguous integer ranges. Changing how an id is
  built or serialised breaks geospatial indexing silently (the
  [htm](https://github.com/Kronuz/htm) library, `Serialise::trixel_id` in
  `src/serialise.cc`).
- **An index path is well-formed UTF-8 with no control bytes, NUL or backslash**
  (`src/index_path.h`). The system calls that act on a path read it as a C string,
  and the storage layer cannot round-trip the rest.

## Going deeper

- [AGENTS.md](https://github.com/Kronuz/Xapiand/blob/master/AGENTS.md): the
  working guide, with the known traps.
- [docs/mechanisms/](https://github.com/Kronuz/Xapiand/tree/master/docs/mechanisms):
  one note per mechanism.
- strata's [ARCHITECTURE.md](https://github.com/Kronuz/strata/blob/main/docs/ARCHITECTURE.md)
  and prospect's [ARCHITECTURE.md](https://github.com/Kronuz/prospect/blob/main/docs/ARCHITECTURE.md):
  the engine.
