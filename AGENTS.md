# AGENTS.md — working in the Xapiand codebase

Orientation for anyone (human or AI) making changes here. Read
[ARCHITECTURE.md](ARCHITECTURE.md) first for the *what*; this file is the *how*.

## First, the big split: own code vs. vendored

`src/` is Xapiand's own code plus two vendored trees. **Do not "fix" or refactor
vendored code**; treat it as read-only:

```
src/xapian/      Xapian fork (GPL): the search core. Large; integration point only.
src/yaml/        libyaml, for the YAML bridge (src/y2j/).
```

Everything else that used to be vendored (msgpack-c, fmt, RapidJSON, LZ4, libev, TCLAP,
cppcodec, ChaiScript) and the utility code that grew here now come from the extracted
`Kronuz/*` libraries via CMake `FetchContent`. A bug in one of those is fixed in its own
repository, not here; `src/xapiand_*_trace.h` and friends are the seams that wire them
to Xapiand's logging.

Everything else under `src/` is Xapiand's own. When grepping for a bug or a
feature, scope to the own-code subsystems unless you're tracing a call *into*
Xapian.

`docs/discussions/` is a vendored copy of the complete Kronuz Discussions package. Its
tracked files must remain byte-identical with `~/code/KronuzBlog/Kronuz.github.io/discussions`
and `~/code/KronuzBlog/gmendezb-pages/discussions`. It includes an Astro component, browser
widget, Cloudflare Worker, D1 migrations, tests, and generic documentation. Xapiand does not
currently mount the comments component. Make package changes in one source tree, synchronize
the complete directory, and verify all three copies before committing.

`src/xapian/` is special: it's a *fork* (pristine upstream snapshot + a small
stack of our patches), not a plain read-only bundle. Before upgrading the
vendored Xapian, reconciling our patches, or touching anything under it, read
[XAPIAN_FORK.md](XAPIAN_FORK.md) — the fork model, why each patch exists, and
the upgrade procedure.

## Where things live

| You're touching… | Start in |
|---|---|
| On-disk format, durability, volumes | `src/database/{wal,data,shard}.{cc,h}`; the volume engine is the [storage](https://github.com/Kronuz/storage) library (`src/xapiand_storage_*.h` wire it) |
| Schema handling, field types | `src/database/schema*.{cc,h}`, `src/reserved/` |
| HTTP API / request handling | `src/server/search_{service,views}.*`, `src/server/http_utils.h`; transport is the [http](https://github.com/Kronuz/http) library |
| The event loop / connections | the [reactor](https://github.com/Kronuz/reactor) library; `src/manager.*` owns the services |
| Clustering, discovery, replication | `src/server/discovery.*`, `src/server/raft_delegate.h`, `src/server/{remote,replication}_protocol_*`, `src/manager.*`, `src/node.*`; bus and Raft come from [cluster](https://github.com/Kronuz/cluster) |
| Query languages | `src/query_dsl.*` (JSON/MsgPack); the string parser is the [boolean-parser](https://github.com/Kronuz/boolean-parser) library |
| Aggregations | `src/aggregations/` |
| Geospatial | `src/geospatial/`, `src/multivalue/geospatialrange.*` |
| Value encoding | `src/sortable_serialise.*`, `src/serialise*.{cc,h}`, `src/length.*` |
| Logging | `src/log.h` (category switches); the logger is the [logger](https://github.com/Kronuz/logger) library |
| Scripting | `src/lua/`, `src/script.*` |
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
- **Services on the reactor.** There is no libev and no `Worker` tree any more.
  `XapiandManager` runs a control-plane `reactor::Reactor`; the HTTP, remote, and
  replication services run on the [reactor](https://github.com/Kronuz/reactor)
  library's shared-nothing loops, with blocking work offloaded to its worker pool.
  Never block a reactor thread.
- **MsgPack as the universal value.** JSON, MessagePack, and internal objects are
  all `MsgPack` (from the [msgpack](https://github.com/Kronuz/msgpack) library), a
  copy-on-write wrapper. Reserved keys are
  `$`/`_`-prefixed (`src/reserved/`).
- **Perfect-hash dispatch.** Reserved-word handling (query DSL, aggregations) is
  done with compile-time perfect-hash tables (`phf::make_phf`), not if-ladders.
  Add a keyword by extending both the hash table and its `switch`.
- **Serialization is order-sensitive.** Values stored for range/sort use
  `sortable_serialise` (memcmp order == numeric order). If you change how a value
  is encoded, you change its sort/range semantics — tread carefully and keep the
  encoding monotonic.
- **Splash banner font.** The ASCII wordmark in `banner()` (`src/main.cc`) is
  figlet's **Standard** font (the default), *not* `slant` — `figlet apiand`
  reproduces the letters after the "X" exactly. The "X" itself is hand-stylized
  (the diamond `\ \/ /` … `/_/`), not figlet output, so don't regenerate the whole
  word blind; keep the custom X and only the "apiand" tail is figlet. prism's
  banner uses the same font, straight from `figlet prism` (all-lowercase, no custom
  glyph needed).

## Load-bearing invariants (don't break these)

- **Trixel ids encode the quadtree path 2 bits per level**, which is *why* a
  region is a contiguous integer range and why geo serialization is big-endian
  (the [htm](https://github.com/Kronuz/htm) library, `src/serialise.cc`). Any change to id construction or
  endianness breaks geo indexing silently.
- **One primary shard per logical shard, elected via Raft** (`discovery.cc`).
  Data replication is asynchronous and pull-based; do not assume a write is on
  any replica when it commits. If you touch the commit path, preserve the
  `DB_UPDATED` multicast that triggers replication.
- **The storage write cursor lives in the volume header**; bins are 8-byte
  aligned and a volume is capped at 32 GiB. Don't write a bin without going
  through the [storage](https://github.com/Kronuz/storage) library's
  `Storage::write`/`write_buffer` (the double-buffer alignment dance).
- **WAL replay must be idempotent w.r.t. revisions** (`wal.cc` checks
  `revision == db_revision`). Don't add WAL ops that can't be safely re-applied.

## Traps (things that have already bitten, per the code review)

- Storage/WAL integrity checking is only as strong as the framing you give it: the
  storage library checksums its v2 volume header and hands each record's XXH32 to
  your footer type, but the reference footer stores none. Check what Xapiand's
  footers verify before relying on corruption detection.
- WAL durability is **async by default**; "it's in the WAL" is not "it's on
  disk" unless the shard is flagged synchronous.
- `query_dsl.cc::process` recurses over the query object with no depth cap, so a
  deeply nested query can exhaust the stack — be careful adding nesting, and
  consider a guard if you're in there. (The boolean-parser library builds its tree
  iteratively; Xapiand reads its RPN queue directly anyway.)

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

## Releasing & the changelog

- The changelog is `docs/src/content/docs/changelog.md` ([Keep a
  Changelog](https://keepachangelog.com/) + SemVer); the root `CHANGELOG` is a
  symlink to it. It's a curated, user-facing summary — terse bullets grouped under
  `Added` / `Changed` / `Fixed` / `Removed`, bold for key terms — not a commit dump.
- **Keep `[Unreleased]` filled as you go.** When a change is worth telling a user
  about, add its bullet to `[Unreleased]` in the same PR. Skip pure-internal churn
  (CI plumbing, no-op refactors). This way cutting a release is nearly free and the
  log never falls behind (it did once, alpha.2–4 were backfilled).
- **Cutting `vX.Y.Z`:**
  1. In the changelog, rename `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD` and add
     a fresh empty `## [Unreleased]` above it.
  2. Bump `PACKAGE_VERSION` in `CMakeLists.txt` to match (it's the fallback; the real
     version comes from `git describe --tags`, so they must agree).
  3. Commit, then tag on HEAD: `git tag vX.Y.Z` (lightweight is fine — the build uses
     `git describe --tags`).
  4. Push `master` then the tag. The tag push triggers Release / FreeBSD / Bottles.
- The Homebrew formula (`contrib/homebrew/xapiand.rb`) is **head-only on purpose** —
  never put a pinned `url`/`sha256` in it. The bottling workflow injects the released
  tag's url + fresh source sha256 at build time, so there is nothing to bump there.
