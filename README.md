# Xapiand

[![Build Status][travis-image]][travis-url]

## A RESTful Search Engine

Xapiand is *A Modern Highly Available Distributed RESTful Search and Storage
Engine built for the Cloud and with Data Locality in mind*. It takes JSON
(or MessagePack) documents and indexes them efficiently for later retrieval.

Official site is at [https://kronuz.io/Xapiand](https://kronuz.io/Xapiand)

---

## What it is

A single C++17 binary that gives you, out of the box:

- A **RESTful HTTP API** over a schemaless document store (JSON or MessagePack in, JSON/MessagePack out).
- **Full-text search**, built on a customized in-tree fork of [Xapian](https://xapian.org/).
- **Geospatial** indexing and queries via a Hierarchical Triangular Mesh (HTM): points, circles, polygons, and collections become numeric range queries on the sphere.
- **Range and numeric** queries through order-preserving ("sortable") value encoding.
- **Aggregations** in the Elasticsearch style (buckets + metrics), computed as a match-spy and mergeable across shards.
- **Clustering**: per-shard primary election over Raft, with asynchronous, pull-based replication between nodes.
- **Durable storage**: a Haystack-inspired append-only volume format with an optional write-ahead log.

The design goal throughout is to keep the moving pieces few and the defaults
sane: one binary does something useful immediately, and still scales out across
a cluster when you need it to.

> **A note on scope.** This repository vendors several third-party libraries
> under `src/` (the Xapian fork, msgpack, rapidjson, fmt, lz4, libev, tclap,
> prometheus-cpp, yaml, cppcodec, ChaiScript). The interesting, original code is
> everything else. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full map of
> what is Xapiand's own and what is bundled.

## Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** — a deep tour of the subsystems
  (storage, clustering, search, geospatial, logging, the utility layer), the
  self-contained components worth reusing, and a catalog of bugs and risks
  found while reading the code.
- **[AGENTS.md](AGENTS.md)** — orientation for anyone (human or AI) working in
  this codebase: where things live, the conventions, the load-bearing
  invariants, and the traps.
- **[SCHEDULER.md](SCHEDULER.md)** — a focused study of the lock-free
  stash / scheduler / debouncer engine that drives the logger thread and every
  background fsync/commit/replication trigger, and how to lift it into
  standalone libraries.
- **[EXTRACTION.md](EXTRACTION.md)** — a complete catalog of the reusable
  components hiding in this codebase, grouped into extraction waves, each with a
  concrete split-out plan and licensing notes.

## Building

Requires a C++17 compiler and CMake ≥ 3.12.

```sh
mkdir build && cd build
cmake ..
make
```

Notable CMake options (all default on unless noted):

| Option | Default | What it does |
|---|---|---|
| `CLUSTERING` | ON | Remote clustering (discovery + replication). |
| `DATABASE_WAL` | ON | Per-shard write-ahead log. |
| `DATA_STORAGE` | ON | Haystack-style document storage volumes. |
| `TRACEBACKS` | OFF (ON in Debug) | Pretty backtraces + cross-thread stack dumps. |
| `ASSERTS` | OFF (ON in Debug) | Internal assertions. |
| `TRACKED_MEM` | OFF | Allocator that attributes memory to call sites. |
| `BUILD_TESTS` / `BUILD_BENCHMARKS` | OFF | Build the test / benchmark suites. |

## License

[MIT](LICENSE) — Copyright © 2015–2019 Dubalu LLC.

Note that the bundled Xapian fork under `src/xapian/` carries Xapian's own
(GPL) license; see [ARCHITECTURE.md](ARCHITECTURE.md#licensing-note) before
reusing code from this tree.

[travis-image]: https://travis-ci.org/Kronuz/Xapiand.svg
[travis-url]: https://travis-ci.org/Kronuz/Xapiand
