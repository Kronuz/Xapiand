---
title: "Dependencies"
---

Xapiand is assembled from **57 standalone Kronuz libraries** and **8 third-party
ones**, all fetched at CMake configure time through `FetchContent`, each pinned
to an exact commit or release. Most of the Kronuz libraries were cut out of
Xapiand's own tree, so each one is small, focused and tested on its own; the
search engine, [strata](https://github.com/Kronuz/strata) and
[prospect](https://github.com/Kronuz/prospect), is two of them. Because they are
fetched, **the first configure needs a network connection** (see
[Building from Sources](/Xapiand/building)).

This page lists them by role, with a link to each repository, and shows how the
composite ones depend on each other.

## Transitive dependency tree

Most libraries are leaves that Xapiand consumes directly. A few are composite:
they fetch their own dependencies at their own pins, which is where the tree gets
its depth.

```text
Xapiand
├── prospect ──── strata, analysis, docset*, bitpack*, fst*
├── strata ────── task, threadpool, compressors, hashes, journal*, completion*,
│                 termdict*, fst*, fusefilter*, residency*, varint*, crc32c*
├── queryparser ─ analysis
├── analysis ──── snowball, unicode
├── http ──────── reactor, task, http-parser, compressors, radix-router*, asio
├── reactor ───── task, asio
├── task ──────── asio
├── msgpack ───── atomic-shared-ptr, constexpr-phf, enum-reflection, hashes,
│                 located-exception, repr, strict-stox, RapidJSON
├── storage ───── compressors, errno-names, strict-stox, stringified
├── logger ────── scheduler, term-color
├── scheduler ─── threadpool, random, stash*
├── base-x ────── uinteger_t*
└── 40-odd leaf libraries, consumed directly (the tables below)

*  fetched only by the library to its left, at that library's pin
```

## The engine

| Library | Purpose |
|---|---|
| [strata](https://github.com/Kronuz/strata) | Segment store: one index per shard, its write buffer, segments, manifests, commits, merges, and the catalog of open indexes |
| [prospect](https://github.com/Kronuz/prospect) | The search layout on strata: postings, positions, doc values, BM25 scoring, top-k, sorting, collapse |
| [task](https://github.com/Kronuz/task) | C++20 coroutine tasks: executor affinity, time-slice budgets, cancellation, async mutexes and events |
| [analysis](https://github.com/Kronuz/analysis) | Text analysis: words, case folding, stems, stop words, CJK n-grams, positions |
| [snowball](https://github.com/Kronuz/snowball) | The Snowball stemmers, all 38 algorithms |
| [unicode](https://github.com/Kronuz/unicode) | Unicode categories, lowercase mappings and UTF-8 decoding |
| [queryparser](https://github.com/Kronuz/queryparser) | The query-string grammar of text fields, analysed like the index terms |
| [boolean-parser](https://github.com/Kronuz/boolean-parser) | The boolean `q=` query language (AND, OR, NOT, XOR, MAYBE) |

## Runtime and storage

| Library | Purpose |
|---|---|
| [threadpool](https://github.com/Kronuz/threadpool) | `WorkPool` and `WorkGroup`: the pool of named threads that runs chores and merges |
| [scheduler](https://github.com/Kronuz/scheduler) | Timer-wheel scheduler and debouncer |
| [storage](https://github.com/Kronuz/storage) | Crash-safe append-only volumes (32 GiB each) for the WAL and the blobs |
| [io](https://github.com/Kronuz/io) | POSIX file and socket I/O (EINTR-safe, portable) |
| [fs](https://github.com/Kronuz/fs) | Filesystem utilities (stat, mkdir, walk) |
| [system](https://github.com/Kronuz/system) | OS facts: descriptor limits, CPU count, memory |

## Network and HTTP

| Library | Purpose |
|---|---|
| [reactor](https://github.com/Kronuz/reactor) | Asio server runtime: shared-nothing loops, admission, TLS, lag probes, phased teardown |
| [http](https://github.com/Kronuz/http) | HTTP/1.1 on reactor: routing, compression, ranges, body streams |
| [http-parser](https://github.com/Kronuz/http-parser) | HTTP parser that accepts Xapiand's custom verbs |
| [http-log](https://github.com/Kronuz/http-log) | Colored request and response logs |
| [url-parser](https://github.com/Kronuz/url-parser) | URL paths (index, id, selector, `:command`) and query parameters |

## Serialization, identity and time

| Library | Purpose |
|---|---|
| [msgpack](https://github.com/Kronuz/msgpack) | The copy-on-write `MsgPack` value, JSON adaptors, JSON Patch |
| [cuuid](https://github.com/Kronuz/cuuid) | UUID generation, parsing and the condensed binary form |
| [datetime](https://github.com/Kronuz/datetime) | Date and time parsing and formatting |
| [epoch](https://github.com/Kronuz/epoch) | Unix time as an integer in any unit |
| [endian](https://github.com/Kronuz/endian) | Endian detection and swapping |
| [base-x](https://github.com/Kronuz/base-x) | Base-N encodings (Base32, Base58, …) |
| [strict-stox](https://github.com/Kronuz/strict-stox) | Strict string-to-number conversion |

## Geography and text metrics

| Library | Purpose |
|---|---|
| [htm](https://github.com/Kronuz/htm) | Hierarchical Triangular Mesh: shapes on the sphere as integer ranges |
| [cartesian](https://github.com/Kronuz/cartesian) | Cartesian coordinate math |
| [soundex](https://github.com/Kronuz/soundex) | Soundex phonetic encoding (EN, FR, DE, ES) |
| [double-metaphone](https://github.com/Kronuz/double-metaphone) | Double Metaphone phonetic encoding |
| [string-similarity](https://github.com/Kronuz/string-similarity) | Levenshtein, Jaro-Winkler, Jaccard, Sørensen-Dice, LCS |

## Logging and diagnostics

| Library | Purpose |
|---|---|
| [logger](https://github.com/Kronuz/logger) | Structured logging with categories and lazy arguments |
| [traceback](https://github.com/Kronuz/traceback) | Crash backtraces and signal handling |
| [errno-names](https://github.com/Kronuz/errno-names) | errno to name mapping |
| [located-exception](https://github.com/Kronuz/located-exception) | Source location in exceptions |
| [term-color](https://github.com/Kronuz/term-color) | ANSI terminal colors |

## Hashing, randomness and compression

| Library | Purpose |
|---|---|
| [hashes](https://github.com/Kronuz/hashes) | Non-cryptographic hashes (FNV-1a, xxHash, …) |
| [md5](https://github.com/Kronuz/md5) | MD5 |
| [sha256](https://github.com/Kronuz/sha256) | SHA-256 |
| [random](https://github.com/Kronuz/random) | Random helpers over one per-thread generator |
| [compressors](https://github.com/Kronuz/compressors) | LZ4, zstd and deflate codecs |

## Strings

| Library | Purpose |
|---|---|
| [strings](https://github.com/Kronuz/strings) | String utilities (split, trim, format) |
| [split](https://github.com/Kronuz/split) | Lazy string splitting |
| [repr](https://github.com/Kronuz/repr) | Readable representation of values |
| [stringified](https://github.com/Kronuz/stringified) | One parameter type for any string, with a `c_str()` when a C API needs one |
| [char-classify](https://github.com/Kronuz/char-classify) | Character classification tables |
| [static-string](https://github.com/Kronuz/static-string) | Compile-time strings |

## Compile-time and small utilities

| Library | Purpose |
|---|---|
| [constexpr-phf](https://github.com/Kronuz/constexpr-phf) | Compile-time minimal perfect hash for keyword dispatch |
| [enum-reflection](https://github.com/Kronuz/enum-reflection) | Enum names and lookups |
| [utype](https://github.com/Kronuz/utype) | `toUType` (an enum to its underlying type) |
| [iterators](https://github.com/Kronuz/iterators) | Lazy `transform`, `chain` and `reversed` adaptors |
| [atomic-shared-ptr](https://github.com/Kronuz/atomic-shared-ptr) | Atomic `shared_ptr` |
| [allocators](https://github.com/Kronuz/allocators) | STL allocators, a node pool, byte-budgeted resources, heap tracking |
| [lru-cache](https://github.com/Kronuz/lru-cache) | LRU cache with time-to-live |
| [math](https://github.com/Kronuz/math) | Integer helpers: non-negative modulo, saturating add and subtract |
| [nanosleep](https://github.com/Kronuz/nanosleep) | Portable nanosleep |
| [namegen](https://github.com/Kronuz/namegen) | Fantasy name generator (the node's default name) |

## Third-party libraries

These come from outside the Kronuz libraries, also through `FetchContent`.

| Library | Version | Purpose |
|---|---|---|
| [Asio](https://github.com/chriskohlhoff/asio) | 1.36.0 | Networking and the event loops, standalone (no Boost) |
| [CLI11](https://github.com/CLIUtils/CLI11) | 2.4.2 | Command-line option parsing |
| [cppcodec](https://github.com/tplgy/cppcodec) | 0.2 | Base-N codecs |
| [LZ4](https://github.com/lz4/lz4) | 1.8.1 | LZ4 compression |
| [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) | 1.2.4 | The `/:metrics` exposition (core only) |
| [RapidJSON](https://github.com/Tencent/rapidjson) | a pinned commit | JSON parsing and writing |
| [libyaml](https://github.com/yaml/libyaml) | 0.2.5 | YAML bodies |
| [Lua](https://www.lua.org/) | 5.4.7 | Document scripts, through the C API, with a patch that meters their work |

The system provides the rest: zstd, libuuid, zlib, the threads library and, with
HTTPS on (the default), OpenSSL 3.
