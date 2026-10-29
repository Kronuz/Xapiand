---
title: "Architecture Overview"
---

Xapiand is a search and storage server, built on a modern C++20
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
**storage** subsystem and the **search core**.

```d2 alt="Xapiand's layered architecture: clients reach the application layer, which drives the storage subsystem and the search core, and storage feeds the search core"
direction: down
Clients: "Clients\n(REST / MessagePack over HTTP)"
App: "Application Layer\n(Asio reactor, HTTP API,\nrouting, Lua scripting)"
Store: "Storage\n(WAL, shards, volumes)"
Search: "Search Core\n(strata, query\nDSL, aggregations)"
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

### Storage

The persistent layer keeps data crash-safe:

- **WAL** — a write-ahead log for durability and crash recovery; replay is
  idempotent against the database revision.
- **Shards** — logical partitions of an index, each a strata index in its own
  `strata/` directory.
- **Volumes** — a crash-safe binary format ([storage](https://github.com/Kronuz/storage)):
  8-byte-aligned bins, a header/footer for recovery, capped near 34 GB.

### Search Core

[strata](https://github.com/Kronuz/strata) is the search engine, reached through
[prospect](https://github.com/Kronuz/prospect), its search layout, behind the
engine seam in `src/engine/`. It runs in-process as a library. Text analysis comes
from the [analysis](https://github.com/Kronuz/analysis) and
[snowball](https://github.com/Kronuz/snowball) libraries and text queries from
[queryparser](https://github.com/Kronuz/queryparser); the query DSL and the
aggregations (range, cardinality, geospatial, nested) are Xapiand's own code.

## Where to Go Next

- **Build it:** [Building from Sources](/Xapiand/building) — requirements, the
  build process, sanitizers, and the full CMake flag reference.
- **The libraries:** [Dependencies](/Xapiand/architecture/dependencies) — the
  ~60 fetched libraries and how they depend on each other.
- **The code:** [Internals](/Xapiand/architecture/internals) — source layout,
  conventions, design principles, and invariants.
