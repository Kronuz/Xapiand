---
title: "Dependencies"
---

Xapiand is assembled from **59 standalone Kronuz libraries** plus a handful of
third-party ones, all pulled in at CMake configure time through `FetchContent`.
Most were extracted from Xapiand's own tree into their own repositories, so each
one is small, focused, and testable on its own. Because they're fetched, **a
network connection is required the first time you configure** (see
[Building from Sources](/Xapiand/building)).

This page lists them by role, with a link to each repository, and shows how the
composite ones depend on each other.

## Transitive Dependency Tree

Most libraries are leaves that Xapiand consumes directly. A handful are
*composite*: they pull their own dependencies, which is where the tree gets its
depth. `asio` and `rapidjson` are the only third-party libraries reached purely
transitively.

```text
Xapiand
├── msgpack ──── atomic-shared-ptr, enum-reflection, strict-stox,
│                located-exception, perfect-hash, hashes, repr, rapidjson*
├── http ─────── radix-router, http-parser, compressors, reactor, asio*
│   └── reactor ──── asio*
├── cluster ──── reactor ──── asio*
├── storage ──── compressors, errno-names, strict-stox, stringified
├── flume ────── compressors
└── (50+ leaf libraries consumed directly — see the tables below)

*  third-party, reached only transitively:
     rapidjson  → github.com/Tencent/rapidjson
     asio       → github.com/chriskohlhoff/asio (1.36.0)
```

## Core Infrastructure

The runtime foundation: threading, scheduling, logging, storage, and OS
abstraction.

| Library | Purpose |
|---------|---------|
| [threadpool](https://github.com/Kronuz/threadpool) | C++20 thread pool with named threads |
| [scheduler](https://github.com/Kronuz/scheduler) | Timer-wheel scheduler and debouncer |
| [logger](https://github.com/Kronuz/logger) | Structured logging with categories |
| [traceback](https://github.com/Kronuz/traceback) | Crash backtraces + signal handling (atos / addr2line) |
| [storage](https://github.com/Kronuz/storage) | Crash-safe binary volumes with WAL |
| [io](https://github.com/Kronuz/io) | POSIX file/socket I/O (EINTR-safe, portable) |
| [fs](https://github.com/Kronuz/fs) | Filesystem utilities (stat, mkdir, walk) |
| [system](https://github.com/Kronuz/system) | OS abstraction (fd limits, CPU count, RSS/VSZ) |
| [cuuid](https://github.com/Kronuz/cuuid) | UUID generation and parsing |

## Network, Serialization & Clustering

| Library | Purpose |
|---------|---------|
| [reactor](https://github.com/Kronuz/reactor) | Asio server runtime (shared-nothing reactor pool) |
| [http](https://github.com/Kronuz/http) | HTTP/1.1 framework (routing, compression, ranges) |
| [http-parser](https://github.com/Kronuz/http-parser) | HTTP parser (accepts custom REST verbs) |
| [radix-router](https://github.com/Kronuz/radix-router) | Radix-tree HTTP path router |
| [msgpack](https://github.com/Kronuz/msgpack) | MessagePack + RapidJSON adaptor + JSON-Patch |
| [url-parser](https://github.com/Kronuz/url-parser) | URL / URI parsing |
| [cluster](https://github.com/Kronuz/cluster) | Cluster membership and coordination |
| [flume](https://github.com/Kronuz/flume) | Message streaming / fan-out between nodes |

## Search & Text Metrics

| Library | Purpose |
|---------|---------|
| [soundex](https://github.com/Kronuz/soundex) | Soundex phonetic encoding (EN/FR/DE/ES) |
| [string-similarity](https://github.com/Kronuz/string-similarity) | Levenshtein, Jaro-Winkler, Jaccard, Sørensen-Dice, LCS |
| [double-metaphone](https://github.com/Kronuz/double-metaphone) | Double Metaphone phonetic encoding |
| [htm](https://github.com/Kronuz/htm) | Hierarchical Triangular Mesh (geospatial indexing) |

## Data Structures

| Library | Purpose |
|---------|---------|
| [queue](https://github.com/Kronuz/queue) | Bounded blocking MPMC double-ended queue |
| [stash](https://github.com/Kronuz/stash) | Hash table |
| [bloom-filter](https://github.com/Kronuz/bloom-filter) | Bloom filter |
| [lru-cache](https://github.com/Kronuz/lru-cache) | LRU cache with time-to-live |

## Numeric & Encoding

| Library | Purpose |
|---------|---------|
| [endian](https://github.com/Kronuz/endian) | Endian detection and swapping |
| [utype](https://github.com/Kronuz/utype) | `toUType` (enum to underlying type) |
| [base-x](https://github.com/Kronuz/base-x) | Base-N encoding (Base32, Base58, …) |
| [uinteger_t](https://github.com/Kronuz/uinteger_t) | Arbitrary-precision unsigned integers |
| [cartesian](https://github.com/Kronuz/cartesian) | Cartesian coordinate math |
| [math](https://github.com/Kronuz/math) | Math helpers (abs, min, max, gcd) |

## Hashing, Randomness & Compression

| Library | Purpose |
|---------|---------|
| [md5](https://github.com/Kronuz/md5) | MD5 hashing |
| [sha256](https://github.com/Kronuz/sha256) | SHA-256 hashing |
| [hashes](https://github.com/Kronuz/hashes) | Assorted non-cryptographic hash functions |
| [random](https://github.com/Kronuz/random) | Fast PRNG (xorshift128+) |
| [compressors](https://github.com/Kronuz/compressors) | LZ4 / zstd / deflate codecs |

## Text Processing

| Library | Purpose |
|---------|---------|
| [strings](https://github.com/Kronuz/strings) | String utilities (split, trim, format) |
| [split](https://github.com/Kronuz/split) | String splitting and tokenization |
| [escape](https://github.com/Kronuz/escape) | Character escaping (URL, JSON, HTML) |
| [repr](https://github.com/Kronuz/repr) | Human-readable representation of values |
| [stringified](https://github.com/Kronuz/stringified) | Stringification of types |
| [char-classify](https://github.com/Kronuz/char-classify) | Unicode character classification |
| [strict-stox](https://github.com/Kronuz/strict-stox) | Strict string-to-number conversion |
| [times](https://github.com/Kronuz/times) | ISO 8601 duration parsing/formatting |
| [datetime](https://github.com/Kronuz/datetime) | Date/time parsing and formatting |

## Time, System & Compile-Time Helpers

| Library | Purpose |
|---------|---------|
| [epoch](https://github.com/Kronuz/epoch) | Unix epoch / timestamp utilities |
| [nanosleep](https://github.com/Kronuz/nanosleep) | Portable nanosleep |
| [errno-names](https://github.com/Kronuz/errno-names) | Errno → name mapping |
| [perfect-hash](https://github.com/Kronuz/perfect-hash) | Compile-time perfect hash generation |
| [enum-reflection](https://github.com/Kronuz/enum-reflection) | Enum name reflection |
| [lazy](https://github.com/Kronuz/lazy) | Lazy static initialization |
| [atomic-shared-ptr](https://github.com/Kronuz/atomic-shared-ptr) | Atomic `shared_ptr` |
| [iterators](https://github.com/Kronuz/iterators) | Iterator helpers |
| [allocators](https://github.com/Kronuz/allocators) | Custom memory allocators |
| [located-exception](https://github.com/Kronuz/located-exception) | Source location in exceptions |
| [static-string](https://github.com/Kronuz/static-string) | Compile-time string handling |

## Miscellaneous

| Library | Purpose |
|---------|---------|
| [term-color](https://github.com/Kronuz/term-color) | ANSI terminal colors |
| [fantasyname](https://github.com/Kronuz/fantasyname) | Random name generator (Markov) |
| [boolean-parser](https://github.com/Kronuz/boolean-parser) | Boolean expression parser |

## Third-Party Libraries

These are vendored from outside the Kronuz ecosystem, also via `FetchContent`.

| Library | Source | Purpose |
|---------|--------|---------|
| [RapidJSON](https://github.com/Tencent/rapidjson) | Tencent | Fast JSON parser/writer |
| [cppcodec](https://github.com/tplgy/cppcodec) | tplgy | Base-N codecs |
| [CLI11](https://github.com/CLIUtils/CLI11) | CLIUtils | Command-line argument parsing |
| [LZ4](https://github.com/lz4/lz4) | lz4 | LZ4 compression |
| [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) | jupp0r | Prometheus metrics |
| [sol2](https://github.com/ThePhD/sol2) | ThePhD | Lua C++ bindings |
| [Lua](https://www.lua.org/) | lua.org | Scripting engine (5.4.7) |
| [asio](https://github.com/chriskohlhoff/asio) | chriskohlhoff | Networking (pulled transitively by `http` / `reactor`) |
