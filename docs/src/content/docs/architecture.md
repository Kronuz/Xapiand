---
title: "Architecture Overview"
---

Xapiand is a distributed search and storage server, built on a modern C++20
runtime and assembled from a large set of small, standalone libraries. This page
is the high-level map: the layers, how a request flows through them, and where to
read more.

Two companion pages go deeper:

- [Dependencies](/Xapiand/architecture/dependencies) — every extracted library,
  with links and a transitive dependency tree.
- [Internals](/Xapiand/architecture/internals) — the source layout, the design
  principles, and the load-bearing invariants you must not break.

## System Overview

Xapiand is organized into three layers on top of the network runtime. Clients
speak HTTP (JSON or MessagePack) to the **application layer**, which drives the
**storage & replication** subsystem and the **search core**.

```d2 alt="Xapiand's layered architecture: clients reach the application layer, which drives the storage subsystem and the search core, and storage feeds the search core"
direction: down
Clients: "Clients\n(REST / MessagePack over HTTP)"
App: "Application Layer\n(Asio reactor, HTTP API,\nrouting, Lua scripting)"
Store: "Storage & Replication\n(WAL, shards, volumes, Raft)"
Search: "Search Core\n(Xapian fork, query\nDSL, aggregations)"
Clients -> App
App -> Store
App -> Search
Store -> Search
```

The whole thing runs as a **single process**. The search engine is a library
call away, not a network hop, so a query is function-call latency plus disk, and
nothing more.

## The Three Layers

### Application Layer

The top layer accepts connections and turns requests into work:

- **Network runtime** — a shared pool of Asio reactors (one `io_context` and
  thread each, shared-nothing), from the [reactor](https://github.com/Kronuz/reactor)
  library.
- **HTTP framework** — request parsing, routing, content negotiation, range and
  compression handling, from [http](https://github.com/Kronuz/http) on top of
  [http-parser](https://github.com/Kronuz/http-parser) and
  [radix-router](https://github.com/Kronuz/radix-router).
- **Request handling** — dispatch to search, index, dump/restore, schema, and
  info operations (`src/server/http*`).
- **Scripting** — custom analytics in [Lua](https://www.lua.org/) via
  [sol2](https://github.com/ThePhD/sol2).

### Storage & Replication

The persistent layer keeps data crash-safe and moves it between nodes:

- **WAL** — a write-ahead log for durability and crash recovery; replay is
  idempotent against the database revision.
- **Shards** — logical partitions, each with its own Xapian database and
  replication state; one primary per shard, elected via Raft.
- **Volumes** — a crash-safe binary format ([storage](https://github.com/Kronuz/storage)):
  8-byte-aligned bins, a header/footer for recovery, capped near 34 GB.
- **Replication** — asynchronous and pull-based. A write commits locally and
  multicasts `DB_UPDATED`; replicas pull when they discover the change.

### Search Core

The [Xapian 2.0.0 fork](https://github.com/Kronuz/xapian) (`src/xapian/`) is the
search engine: indexing, retrieval, query parsing, and aggregations (range,
cardinality, geospatial, nested). It runs in-process as a library. See
[Building the Xapian Library](/Xapiand/building-xapian) to compile it standalone,
and the project's `XAPIAN_FORK.md` for the fork model and patch stack.

## Where to Go Next

- **Build it:** [Building from Sources](/Xapiand/building) — requirements, the
  build process, sanitizers, and the full CMake flag reference.
- **The libraries:** [Dependencies](/Xapiand/architecture/dependencies) — the
  ~60 fetched libraries and how they depend on each other.
- **The code:** [Internals](/Xapiand/architecture/internals) — source layout,
  conventions, design principles, and invariants.
