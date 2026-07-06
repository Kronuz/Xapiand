# AGENTS.md — working in the Xapiand codebase

Orientation for anyone (human or AI) making changes here. Read
[ARCHITECTURE.md](ARCHITECTURE.md) first for the *what*; this file is the *how*.

## First, the big split: own code vs. vendored

`src/` mixes Xapiand's own code with bundled third-party libraries. **Do not
"fix" or refactor vendored code** — treat these as read-only dependencies:

```
src/xapian/      Xapian fork (GPL) — the search core. Large; integration point only.
src/msgpack/     msgpack-c                 src/fmt/        {fmt}
src/rapidjson/   RapidJSON                 src/lz4/        LZ4
src/ev/          libev                     src/tclap/      CLI parsing
src/yaml/        libyaml
src/cppcodec/    base-N codecs            src/chaiscript/ ChaiScript
```

Everything else under `src/` is Xapiand's own. When grepping for a bug or a
feature, scope to the own-code subsystems unless you're tracing a call *into*
Xapian.

`src/xapian/` is special: it's a *fork* (pristine upstream snapshot + a small
stack of our patches), not a plain read-only bundle. Before upgrading the
vendored Xapian, reconciling our patches, or touching anything under it, read
[XAPIAN_FORK.md](XAPIAN_FORK.md) — the fork model, why each patch exists, and
the upgrade procedure.

## Where things live

| You're touching… | Start in |
|---|---|
| On-disk format, durability, volumes | `src/storage.h`, `src/database/{wal,data,shard}.{cc,h}` |
| Schema handling, field types | `src/database/schema*.{cc,h}`, `src/reserved/` |
| HTTP API / request handling | `src/server/http*.{cc,h}`, `src/url_parser.*` |
| The event loop / client lifecycle | `src/worker.{cc,h}`, `src/server/base_{client,server}.*` |
| Clustering, discovery, replication | `src/server/discovery.*`, `src/server/replication_protocol*`, `src/manager.*`, `src/node.*` |
| Query languages | `src/query_dsl.*` (JSON/MsgPack), `src/booleanParser/` (string) |
| Aggregations | `src/aggregations/` |
| Geospatial | `src/geospatial/`, `src/multivalue/geospatialrange.*` |
| Value encoding | `src/sortable_serialise.*`, `src/serialise*.{cc,h}`, `src/length.*` |
| Logging | `src/logger.*`, `src/log.h` (category switches) |
| Small utilities | top-level `src/*.hh` / `*.h` (see the table in ARCHITECTURE.md) |

Entry point is `src/main.cc`; the process is orchestrated by
`XapiandManager` (`src/manager.*`).

## Build & toggles

```sh
mkdir build && cd build && cmake .. && make
```

C++20, CMake ≥ 3.12. The feature toggles matter when reproducing behavior:
`CLUSTERING`, `DATABASE_WAL`, `DATA_STORAGE` (all ON), and `TRACEBACKS` /
`ASSERTS` (ON in Debug builds). `TRACKED_MEM` swaps in an allocator that
attributes memory to call sites. Tests/benchmarks are off by default
(`BUILD_TESTS`, `BUILD_BENCHMARKS`).

## Verifying changes end-to-end (the docs ARE a test suite)

The documentation examples double as an E2E regression suite — the best safety net
for the library-extraction / de-vendor work, since it exercises the real HTTP API
against a running server. Each `docs/**/*.md` example is a request (a ` ```json `
block: `METHOD /url` + headers + body or `@file` fixture) plus ` ```js `
`pm.test(...)` assertions. `docs_to_postman.py` compiles all of it (~303 requests,
~188 with assertions) into a Postman collection:

```sh
python3 docs_to_postman.py | newman run /dev/stdin     # against a live Xapiand on :8880
```

Requirements: `python3` (the script was ported from Python 2), `newman`
(`npm i -g newman`), a running de-vendored Xapiand on `:8880`, and the fixtures in
`docs/assets/` (`twitter.msgpack` is currently missing — generate or skip that one).
Run it before and after a de-vendor to prove Xapiand still works end-to-end.

This is one of several test layers. For the full picture — unit tests, the functional
E2E above, the multi-node remote/cluster net, load, soak/stress, benchmarks, and the
`harness/run_all.sh` one-shot runner — see [TESTING.md](TESTING.md).

## Conventions you'll see everywhere

- **Logging macros `L_*`.** Logging is pervasive (~2,790 call sites). Most
  category macros (`L_CALL`, `L_DATABASE`, `L_EV`, …) compile to **nothing** by
  default and are switched on by editing the `#define`s near the top of
  `src/log.h`. To trace a subsystem, flip its category there and rebuild. Log
  arguments are evaluated lazily, so it's fine to pass expensive expressions.
- **`Worker` + libev.** Anything that owns a socket or a timer is a `Worker`
  (`src/worker.h`), living in a shared-pointer parent/children tree. Never call
  another worker's methods across threads directly — use the async control
  watchers (`shutdown`, `stop`, `destroy`, `detach`), which run on the owning
  loop. Lifetime is by `shared_ptr`; clients keep themselves alive across the
  loop→pool hand-off with `share_this()`.
- **MsgPack as the universal value.** JSON, MessagePack, and internal objects are
  all `MsgPack` (`src/msgpack.h`), a copy-on-write wrapper. Reserved keys are
  `$`/`_`-prefixed (`src/reserved/`).
- **Perfect-hash dispatch.** Reserved-word handling (query DSL, aggregations) is
  done with compile-time perfect-hash tables (`phf::make_phf`), not if-ladders.
  Add a keyword by extending both the hash table and its `switch`.
- **Serialization is order-sensitive.** Values stored for range/sort use
  `sortable_serialise` (memcmp order == numeric order). If you change how a value
  is encoded, you change its sort/range semantics — tread carefully and keep the
  encoding monotonic.

## Load-bearing invariants (don't break these)

- **Trixel ids encode the quadtree path 2 bits per level**, which is *why* a
  region is a contiguous integer range and why geo serialization is big-endian
  (`src/geospatial/htm.cc`, `src/serialise.cc`). Any change to id construction or
  endianness breaks geo indexing silently.
- **One primary shard per logical shard, elected via Raft** (`discovery.cc`).
  Data replication is asynchronous and pull-based; do not assume a write is on
  any replica when it commits. If you touch the commit path, preserve the
  `DB_UPDATED` multicast that triggers replication.
- **The storage write cursor lives in the volume header**; bins are 8-byte
  aligned and a volume is capped near 34 GB. Don't write a bin without going
  through `Storage::write`/`write_buffer` (the double-buffer alignment dance).
- **WAL replay must be idempotent w.r.t. revisions** (`wal.cc` checks
  `revision == db_revision`). Don't add WAL ops that can't be safely re-applied.

## Traps (things that have already bitten, per the code review)

- The `Cartesian` in-place operators are buggy (`operator^=` corrupts, `operator*`
  mutates) — prefer the free-function/out-of-place versions until fixed
  (`cartesian.cc`). See ARCHITECTURE.md → Bugs.
- Storage/WAL integrity checking is weaker than it looks: the default
  header/footer validation is stubbed and the LZ4 digest is computed but never
  verified. If you rely on corruption detection, wire it up first.
- WAL durability is **async by default**; "it's in the WAL" is not "it's on
  disk" unless the shard is flagged synchronous.
- Parsers (`query_dsl.cc::process`, `BooleanParser.cc::BuildTree`) recurse with
  no depth cap — be careful adding nesting, and consider a guard if you're in
  there anyway.
- `BaseClient` will `sig_exit` the whole process if `total_clients` underflows —
  keep the ctor/dtor counting symmetric.

## Making changes well

- Match the surrounding style: heavy `constexpr`, CRTP for zero-overhead
  polymorphism, `string_view` over copies, RAII for every resource (sockets,
  shard checkouts, locks).
- When adding a field type, reserved word, or aggregation, you'll typically touch
  three places: the reserved vocabulary (`src/reserved/`), the schema/serialise
  layer, and the perfect-hash dispatch. Grep an existing one end-to-end first.
- If you extract a utility to its own repo (the `base-x` / `uinteger_t` /
  `fantasyname` pattern), check the licensing note in ARCHITECTURE.md —
  `sortable_serialise` and anything under `src/xapian/` are GPL, the rest is MIT.
- There's no substitute for flipping on the relevant `L_*` category and watching
  the logs; the instrumentation is already there.
