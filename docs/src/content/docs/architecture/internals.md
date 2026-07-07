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

`src/` mixes Xapiand's own code with a few bundled third-party libraries. The
first rule of the tree: **don't refactor vendored code**. The search core,
`src/xapian/`, is special. It's a *fork* (a pristine upstream snapshot plus a
small stack of patches), not a plain bundle. Before touching anything under it,
read [XAPIAN_FORK.md](https://github.com/Kronuz/Xapiand/blob/master/XAPIAN_FORK.md).

Most of the small utilities that used to live in `src/` have been extracted into
standalone repositories and are pulled back in via `FetchContent` (see
[Dependencies](/Xapiand/architecture/dependencies)). The entry point is
`src/main.cc`, and the process is orchestrated by `XapiandManager`
(`src/manager.*`).

## Where Things Live

| You're touching… | Start in |
|---|---|
| On-disk format, durability, volumes | `src/storage.h`, `src/database/{wal,data,shard}.{cc,h}` |
| Schema handling, field types | `src/database/schema*.{cc,h}`, `src/reserved/` |
| HTTP API / request handling | `src/server/http*.{cc,h}`, `src/url_parser.*` |
| Event loop / client lifecycle | `src/worker.{cc,h}`, `src/server/base_{client,server}.*` |
| Clustering, discovery, replication | `src/server/discovery.*`, `src/server/replication_protocol*`, `src/manager.*`, `src/node.*` |
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

**The `Worker` tree.** Anything that owns a socket or a timer is a `Worker`
(`src/worker.h`), living in a shared-pointer parent/children tree. Never call
another worker's methods across threads directly. Use the async control watchers
(`shutdown`, `stop`, `destroy`, `detach`), which run on the owning loop. Lifetime
is by `shared_ptr`; clients keep themselves alive across the loop→pool hand-off
with `share_this()`.

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

**Async-first replication.** Replication is pull-based and asynchronous. A write
commits locally first; replicas discover and pull when ready. This decouples
commit latency from replica sync. A write is *not* on any replica when it commits.

**Shared-nothing reactor pool.** The network transport runs on N independent Asio
reactors, each owning its own `io_context` and thread. No shared state and no
locks between them.

## Load-Bearing Invariants

These are not safe to "clean up". Each one is load-bearing:

- **Trixel ids encode the quadtree path, 2 bits per level, big-endian.** That's
  *why* a geographic region maps to a contiguous integer range. Any change to id
  construction or endianness breaks geo indexing silently
  (`src/geospatial/htm.cc`, `src/serialise.cc`).
- **One primary shard per logical shard, elected via Raft** (`discovery.cc`).
  Replication is asynchronous and pull-based. If you touch the commit path,
  preserve the `DB_UPDATED` multicast that triggers it.
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
- [XAPIAN_FORK.md](https://github.com/Kronuz/Xapiand/blob/master/XAPIAN_FORK.md)
  — the fork model, why each patch exists, and the upgrade procedure.
