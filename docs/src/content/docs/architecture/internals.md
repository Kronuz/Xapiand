---
title: "Internals"
---

This page is for contributors: how the source tree is organized, the conventions
you'll see everywhere, and the invariants that look like implementation details
but are actually structural. For the layered picture, start with the
[Architecture Overview](/Xapiand/architecture); for the libraries, see
[Dependencies](/Xapiand/architecture/dependencies).

The authoritative, always-current version of this lives in the repository's
[AGENTS.md](https://github.com/Kronuz/Xapiand/blob/master/AGENTS.md) and
[ARCHITECTURE.md](https://github.com/Kronuz/Xapiand/blob/master/ARCHITECTURE.md).

## Source Layout

`src/` is Xapiand's own code. The libraries it builds on, including the search
engine, live in standalone repositories and are pulled in via `FetchContent` (see
[Dependencies](/Xapiand/architecture/dependencies)); a bug in one of them is fixed
in its own repository. The search engine is
[strata](https://github.com/Kronuz/strata), behind the engine seam in
`src/engine/`. The entry point is
`src/main.cc`, and the process is orchestrated by `XapiandManager`
(`src/manager.*`).

## Where Things Live

| You're touching… | Start in |
|---|---|
| On-disk format, durability, volumes | `src/database/{wal,data,shard}.{cc,h}`; the index files are strata's (`src/engine/strata/`) |
| Schema handling, field types | `src/database/schema*.{cc,h}`, `src/reserved/` |
| HTTP API / request handling | `src/server/http*.{cc,h}`, `src/url_parser.*` |
| Event loop / client lifecycle | `src/worker.{cc,h}`, `src/server/base_{client,server}.*` |
| Query languages | `src/query_dsl.*` (JSON/MsgPack), `src/booleanParser/` (string) |
| Aggregations | `src/aggregations/` |
| Geospatial | `src/geospatial/`, `src/multivalue/geospatialrange.*` |
| Value encoding | `src/sortable_serialise.*`, `src/serialise*.{cc,h}`, `src/length.*` |
| Logging | `src/logger.*`, `src/log.h` (category switches) |

## Conventions

**Logging macros `L_*`.** Logging is pervasive (~2,790 call sites). Most category
macros (`L_CALL`, `L_DATABASE`, `L_EV`, …) compile to **nothing** by default and
are switched on by editing the `#define`s near the top of `src/log.h`. To trace a
subsystem, flip its category there and rebuild. Log arguments are evaluated
lazily, so passing expensive expressions is fine.

**Never block a loop.** A request, and the node's own work, is a task on an event loop. A task that
has to wait awaits a timer, an event, an index's turn or a chore (device work on the `WorkPool`); it
never calls `sync_wait`, waits on a condition variable, sleeps or makes a file call. `engine::check_may_block`
aborts a debug build that does, and `harness/async_check.sh --strict` (a CI step) fails on any blocking
primitive that `harness/async_allowlist.tsv` does not classify.

**MsgPack as the universal value.** JSON, MessagePack, and internal objects are
all `MsgPack` (`src/msgpack.h`), a copy-on-write wrapper over a shared,
reference-counted buffer. This keeps copies cheap through the pipeline. Reserved
keys are `$`/`_`-prefixed (`src/reserved/`).

**Perfect-hash dispatch.** Reserved-word handling (query DSL, aggregations) uses
compile-time perfect-hash tables (`phf::make_phf`), not if-ladders. Adding a
keyword means extending both the hash table and its `switch`.

**Serialization is order-sensitive.** Values stored for range/sort use
`sortable_serialise`, where `memcmp` order equals numeric order. Change how a
value is encoded and you change its sort/range semantics, so keep the encoding
monotonic.

## Design Principles

**Copy-on-write values.** `MsgPack` shares one reference-counted buffer and only
copies on mutation, which minimizes copies through the request pipeline and makes
serialization cheap.

**Shared-nothing reactor pool.** The network transport runs on N independent Asio
reactors, each owning its own `io_context` and thread. No shared state and no
locks between them.

## Load-Bearing Invariants

These are not safe to "clean up". Each one is load-bearing:

- **Trixel ids encode the quadtree path, 2 bits per level, big-endian.** That's
  *why* a geographic region maps to a contiguous integer range. Any change to id
  construction or endianness breaks geo indexing silently
  (`src/geospatial/htm.cc`, `src/serialise.cc`).
- **The storage write cursor lives in the volume header.** Bins are 8-byte
  aligned and a volume is capped near 34 GB. Don't write a bin outside
  `Storage::write` / `write_buffer` (the double-buffer alignment dance).
- **WAL replay must be idempotent w.r.t. revisions** (`wal.cc` checks
  `revision == db_revision`). Don't add WAL ops that can't be safely re-applied.

## Going Deeper

- [AGENTS.md](https://github.com/Kronuz/Xapiand/blob/master/AGENTS.md) — the full
  working guide, including known traps and gotchas.
- [ARCHITECTURE.md](https://github.com/Kronuz/Xapiand/blob/master/ARCHITECTURE.md)
  — the C++ codebase deep-dive.
