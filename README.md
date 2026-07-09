# Xapiand

[![CI][ci-image]][ci-url]

## A RESTful Search Engine

Xapiand is *A Modern Highly Available Distributed RESTful Search and Storage
Engine built for the Cloud and with Data Locality in mind*. It takes JSON
(or MessagePack) documents and indexes them efficiently for later retrieval.

Official site is at [https://kronuz.github.io/Xapiand](https://kronuz.github.io/Xapiand)

---

## What it is

A single C++20 binary that gives you, out of the box:

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

> **A note on scope.** This repository still vendors the customized Xapian fork
> and libyaml bridge under `src/`. The old in-tree copies of cuuid, msgpack,
> prometheus-cpp, libev, LZ4, RapidJSON, cppcodec, TCLAP, fmt, and ChaiScript are
> gone; CMake now pulls the extracted `Kronuz/*` libraries and upstream
> prometheus-cpp core with `FetchContent`. See [ARCHITECTURE.md](ARCHITECTURE.md)
> for the full map.

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

Requires a C++20 compiler and CMake ≥ 3.16. Dependencies are fetched
automatically at configure time via CMake `FetchContent` (see below).

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

## Architecture & dependencies

Xapiand runs entirely on the standalone-Asio
[reactor](https://github.com/Kronuz/reactor) runtime (there is no libev), and is
assembled from **59 standalone `Kronuz/*` libraries** plus a few third-party ones,
all wired in by CMake `FetchContent` at configure time. The runtime topology, the
dependency layering diagram, and the **full dependency tree** live in
[ARCHITECTURE.md](ARCHITECTURE.md); see
[Runtime architecture](ARCHITECTURE.md#runtime-architecture) and
[Dependencies](ARCHITECTURE.md#dependencies).

The load-bearing pieces: `reactor` (the Asio server runtime), `cluster` (Bus gossip +
Raft), `http` (HTTP transport), `flume` (framed file transfer), `storage` (append-only
blob store), `io` / `fs` / `system` (POSIX I/O, filesystem, resource introspection),
plus the extracted `cuuid` condensed-UUID value type, the `msgpack` keystone
(msgpack-c + COW `MsgPack` + `xchange` + patcher), a foundation of `strings` /
`repr` / `logger` / `traceback` / `compressors`, and the search-domain libraries.

## License

[MIT](LICENSE) — Copyright © 2015–2026 Dubalu LLC.

Note that the bundled Xapian fork under `src/xapian/` carries Xapian's own
(GPL) license; see [ARCHITECTURE.md](ARCHITECTURE.md#licensing-note) before
reusing code from this tree.

[ci-image]: https://github.com/Kronuz/Xapiand/actions/workflows/ci.yml/badge.svg
[ci-url]: https://github.com/Kronuz/Xapiand/actions/workflows/ci.yml
