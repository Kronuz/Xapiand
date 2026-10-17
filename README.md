# Xapiand

[![CI][ci-image]][ci-url]
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

<p align="center"><img src="docs/public/img/logo-2x.png" alt="Xapiand" width="320"></p>

**A RESTful search and storage engine in one binary.** Send it JSON or MessagePack
documents over HTTP, then search them with full text, geospatial, and range queries.
It runs on one machine and scales out to a cluster.

📖 Official site: [kronuz.github.io/Xapiand](https://kronuz.github.io/Xapiand)

## 📦 Installation

```sh
docker run -p 8880:8880 --rm ghcr.io/kronuz/xapiand:latest      # Linux amd64 / arm64
brew install Kronuz/tap/xapiand && xapiand                     # macOS
```

Each release also ships RPM and DEB packages; see
[Installation](https://kronuz.github.io/Xapiand/installation/), or build from source
below.

## 🚀 Quick start

```sh
curl -X PUT 'localhost:8880/twitter/tweet/1?commit' -H 'Content-Type: application/json' \
  -d '{"user": "Kronuz", "postDate": "2016-11-15T13:12:00", "message": "Trying out Xapiand"}'

curl localhost:8880/twitter/tweet/1                         # get it back
curl -X SEARCH 'localhost:8880/twitter/tweet/?q=user:Kronuz'  # search for it
```

The index is created on first write. Writes become searchable at the next commit,
about a second later; `?commit` waits for it. The
[Quick-start](https://kronuz.github.io/Xapiand/quickstart/) continues with
searches, aggregations, and geo queries.

## ✨ What it is

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
> under `src/`. The old in-tree copies of cuuid, msgpack, prometheus-cpp, libev,
> LZ4, RapidJSON, cppcodec, TCLAP, fmt, ChaiScript, and libyaml are gone; CMake now
> pulls the extracted `Kronuz/*` libraries and the upstream ones with `FetchContent`. See [ARCHITECTURE.md](ARCHITECTURE.md)
> for the full map.

## 📚 Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** — a deep tour of the subsystems
  (storage, clustering, search, geospatial, logging, the utility layer), the
  self-contained components worth reusing, and a catalog of bugs and risks
  found while reading the code.
- **[AGENTS.md](AGENTS.md)** — orientation for anyone (human or AI) working in
  this codebase: where things live, the conventions, the load-bearing
  invariants, and the traps.
- **[testing.md](docs/dev/testing.md)** — the test layers, from unit tests to the docs-as-E2E
  suite and cluster soak runs.
- **[xapian-fork.md](docs/dev/xapian-fork.md)** — the vendored Xapian fork and its patch stack.
- **[scheduler.md](docs/design/scheduler.md)** and **[extraction.md](docs/design/extraction.md)** — the
  (now completed) plans that split the scheduler stack and the rest of the utility
  code into the standalone [Kronuz libraries](https://github.com/Kronuz/cpp-libraries).

## 🛠️ Building

Requires a C++20 compiler and CMake 3.16 or newer. Dependencies are fetched
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

## 🧩 Architecture and dependencies

Xapiand runs entirely on the standalone-Asio
[reactor](https://github.com/Kronuz/reactor) runtime (there is no libev), and is
assembled from **about sixty standalone `Kronuz/*` libraries** plus a few third-party ones,
all wired in by CMake `FetchContent` at configure time. The runtime topology, the
dependency layering diagram, and the **full dependency tree** live in
[ARCHITECTURE.md](ARCHITECTURE.md); see
[Runtime architecture](ARCHITECTURE.md#runtime-architecture) and
[Dependencies](ARCHITECTURE.md#dependencies).

The load-bearing pieces: `reactor` (the Asio server runtime), `cluster` (multicast Bus +
Raft), `http` (HTTP transport), `flume` (framed file transfer), `storage` (append-only
blob store), `io` / `fs` / `system` (POSIX I/O, filesystem, resource introspection),
plus the extracted `cuuid` condensed-UUID value type, the `msgpack` keystone
(msgpack-c + COW `MsgPack` + `xchange` + patcher), a foundation of `strings` /
`repr` / `logger` / `traceback` / `compressors`, and the search-domain libraries.

## 🤝 Contributing

Issues and pull requests are welcome; see the
[contributing guide](https://kronuz.github.io/Xapiand/contributing/) and
[AGENTS.md](AGENTS.md). The documentation examples double as the end-to-end test
suite ([testing.md](docs/dev/testing.md)).

## 📄 License

[MIT](LICENSE) — Copyright © 2015–2026 Dubalu LLC.

Note that the bundled Xapian fork under `src/xapian/` carries Xapian's own
(GPL) license; see [ARCHITECTURE.md](ARCHITECTURE.md#licensing-note) before
reusing code from this tree.

[ci-image]: https://github.com/Kronuz/Xapiand/actions/workflows/ci.yml/badge.svg
[ci-url]: https://github.com/Kronuz/Xapiand/actions/workflows/ci.yml
