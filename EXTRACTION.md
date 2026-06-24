# Extracting reusable libraries from Xapiand

Xapiand is full of small, well-made components that have nothing to do with
search and everything to do with "things you need when writing serious C++."
Three are already out in their own repos — [base-x](https://github.com/Kronuz/base-x),
[uinteger_t](https://github.com/Kronuz/uinteger_t), and
[fantasyname](https://github.com/Kronuz/fantasyname) — and this document is the
plan to do the same for the rest.

It's a complete inventory of the codebase's own (non-vendored) code, grouped into
**extraction waves** by return-on-effort, with a concrete split-out plan for each
piece, a suggested set of repos to create, and the licensing flags to carry. For
the deep technical detail behind any entry, see
[ARCHITECTURE.md](ARCHITECTURE.md); the scheduling stack has its own study in
[SCHEDULER.md](SCHEDULER.md).

## How to read this

- **Difficulty** = how much Xapiand coupling you must cut: *Easy* (header-only or
  a couple of macro stubs), *Moderate* (swap a few helpers / a thread abstraction),
  *Hard* (peel domain glue off a good core).
- **Value** = how broadly useful outside a search engine.
- **License** = MIT unless flagged. A few files carry other authors' notices that
  must travel with them; see the [provenance map](#provenance--license-map).
  Anything under `src/xapian/` is GPL and out of scope here.

## The shared decoupling toolkit

Most of the catalog is unlocked by the same five cuts. Do these once as a
pattern and the per-component work mostly evaporates:

1. **Stub the logging macros.** `L_CALL`, `L_ERR`, `L_DEBUG_HOOK`, `L_EXC`, … all
   compile to `L_NOTHING` by default already (`log.h`). Replace with no-ops or a
   single injectable trace callback. This is the single most common coupling and
   it's mechanical.
2. **Swap `THROW(Type, …)` for standard exceptions.** Many leaf files only touch
   Xapiand exceptions through this macro; `std::runtime_error`/`invalid_argument`
   substitute cleanly. (Or extract the exception type itself — it's Wave 1.)
3. **Replace `strings::format` with `fmt::format` / `std::format`.** The internal
   `strings` helper is just an fmt wrapper.
4. **Delete `ThreadPolicyType`.** It's threaded through the whole thread/scheduler
   stack as a template parameter but `thread.cc` ignores it at runtime (only sets
   the thread name). Replace with a name string.
5. **Drop `opts` reads.** A few components read the global CLI-options struct;
   replace those with constructor parameters or a small config struct.

---

## Wave 1 — header-only, (near-)zero coupling

Quick wins. Mostly copy the file(s), stub a macro or two, done.

| Proposed lib | Source | What it is | Diff | Value | Split plan |
|---|---|---|---|---|---|
| **perfect-hash** | `phf.hh` | Fully `constexpr` minimal perfect hash for integer keys | Easy | High | Copy as-is; **fix the inverted `empty()`** (`phf.hh:1410`). Zero deps. |
| **lru-cache** | `lru.h` | Intrusive LRU + optional TTL with policy-callback eviction | Easy | High | Copy as-is; pure STL. |
| **callable-traits** | `callable_traits.hh` | Full C++17 callable introspection (all qualifier combos) | Easy | High | Copy as-is; already namespaced, zero deps. |
| **errno-names** | `error.hh` + `errnos.h` | errno → symbolic name *and* description, cross-platform X-macro | Easy | High | Copy both verbatim; zero coupling. |
| **located-exception** | `exception.h` + `exception.cc` | Exception that captures func/file/line + lazy fmt, with a `WITHOUT_FMT` fallback | Easy | High | Drop the unused `config.h` include; keep `THROW`/`RETHROW`. (Note: captures *location*, not a stack trace.) |
| **bloom-filter** | `bloom_filter.hh` | Fixed-size double-hashed Bloom filter | Easy | Med | Carries `hashes.hh` (or template the hash). |
| **split** | `split.h` | Lazy `string_view` splitter (full-delimiter + find-first-of) | Easy | High | Copy as-is, or fold into the string toolkit below. |
| **msgpack-xchange** | `xchange/rapidjson.hpp`, `xchange/string_view.hpp`, `xchange/chaiscript.hpp` | msgpack-c adapters for RapidJSON, `string_view`, and ChaiScript | Easy | High (json/sv), Low-Med (chai) | Ship the three as one pack; fix one `THROW`→`throw` in string_view; **keep xpol's co-credit** on the rapidjson header. ChaiScript adapter is an optional module. |

## Wave 2 — small decoupling (stub macros / swap exceptions / fmt)

A short, mechanical decoupling each, following the toolkit above.

| Proposed lib | Source | What it is | Diff | Value | Split plan |
|---|---|---|---|---|---|
| **term-color** | `ansi_color.hh`, `colors.h`, `color_tools.hh` | Compile-time truecolor+256+16 ANSI escapes, graceful downgrade, `hsv2rgb` | Easy | High | Split the `L_*` logging shortcuts out of `colors.h`; inline `ESC`; `strings::format`→`fmt`. |
| **constexpr-strings** | `static_string.hh`, `hashes.hh`, `chars.hh`, `strict_stox.hh` | Compile-time strings, xxHash/FNV/djb2 + string-switch UDLs, table-driven char classification, strict numeric parsing | Easy–Mod | High | Bundle as one "compile-time string toolkit"; swap `THROW` for std exceptions in `strict_stox`. |
| **string-metrics** | `metrics/` (+ `string_metric.h`) | CRTP string-distance kit: Levenshtein, Jaro, Jaro–Winkler, Jaccard, Sørensen–Dice, LCS, soundex-metric | Easy | High | Drop the 3 serialise helpers in `basic_string_metric.h`. |
| **phonetic** | `phonetic/` | Multilingual soundex (EN/FR/DE/ES) | Easy | High | Same serialise-helper cut; **fix the Spanish first-letter bug** (`spanish_soundex.h:113`). Could merge into string-metrics. |
| **memory-stats** | `memory_stats.*` | Portable RAM/swap/disk/inode + per-proc RSS/VSZ | Easy | High | Swap ~10 `L_*`/`error::*` calls for a callback or `fprintf`. Bundle into **sysinfo** with `system.*`. |
| **lz4-stream** | `compressor_lz4.*` | LZ4 block-streaming wrappers (file + data, ring-buffered) | Easy | High | Depends only on vendored lz4 + `io`; note the unused-digest bug (wire up the XXH32 it already computes). |
| **urlcodec** | `url_parser.*` (the `urldecode` + `QueryParser` half) | Configurable percent-decoder + zero-copy query-string parser | Easy–Mod | High | Inline the ~10-line `hexdec`; **leave `PathParser`** (Xapiand URL dialect). |
| **mime-types** | `server/mime_types.*` (+ the nginx table) | Load `mime.types`, map extension → content type | Easy–Mod | Low–Med | Replace `ct_type_t` with `std::pair`, make the path a parameter, drop logging. |
| **alloc-kit** | `allocators.*` | STL allocator over pluggable backend + pool allocator + optional malloc tracking | Easy | Med | Split the global `operator new/delete` override into an opt-in TU; **keep moya-lang's BSD-3 notice**. |

## Wave 3 — coherent subsystems (bundle & lift)

Bigger pieces that stand on their own once a thread abstraction or a few helpers
come along. Several are best shipped as a small dependency tree.

| Proposed lib | Source | What it is | Diff | Value | Split plan |
|---|---|---|---|---|---|
| **stash** | `stash.h` | Lock-free hierarchical timer-wheel slot store (the crown jewel) | Easy | High | Stub trace macros; delete the vestigial `_Ring`/`_CurrentKey` params. See [SCHEDULER.md](SCHEDULER.md). |
| **thread-pool** | `thread.hh`, `threadpool.hh` | Blocking-queue thread pool + CRTP `Thread` (promise-based join) | Easy–Mod | Med | Delete `ThreadPolicyType`; deps `blocking_concurrent_queue.h` + `likely.h`; `strings::format`→`fmt`. |
| **timer-scheduler** | `scheduler.h` (+ stash + thread-pool) | 24h multi-resolution timer wheel + thread; cancellable tasks; inline or pool dispatch | Mod | High | Build on **stash** + **thread-pool**; stub macros. See [SCHEDULER.md](SCHEDULER.md). |
| **debouncer** | `debouncer.h` | Per-key throttle/debounce with randomized force-window | Easy* | High | Rides on **timer-scheduler**; vendor `callable_traits` + 3 `random` helpers. (*after the scheduler is out.) |
| **posix-io** | `io.cc`, `io.hh` | EINTR-retry / partial-IO / fsync / fallocate wrappers + fault injection | Mod | High | Cut `log`/`error`/`exception`; gate or drop the `opts`-driven fault injection; `likely.h`→`[[likely]]`. SQLite-derived fallocate fallback is public-domain. |
| **sysinfo** | `system.*` + `memory_stats.*` | Open/max fd counts, OS/arch probes, RAM/disk/swap/inode stats | Mod | Med–High | Drop the stray `STATE_*` defines in `system.cc`; carry `posix-io`; swap logging. |
| **timeutil** | `times.*`, `nanosleep.h`, `epoch.hh`, `time_point.hh` | `timespec` arithmetic + macOS `clock_gettime` shim + chrono one-liners | Easy | Med | De-dup the doubled `nanosleep`; **keep MM Weiss's BSD-3** on the macOS shim (guard it behind legacy-macOS). |
| **traceback** | `traceback.*` | Symbolized backtraces, cross-thread stack dump (signal), `__cxa_throw` capture | Mod | Med | The backtrace/symbolize core lifts cleanly; leave the global allocator interposition out (it's invasive). |
| **libev-reactor** | `worker.*` | shared_ptr lifetime tree + cross-loop async control + cooperative GC of finished workers | Mod | High | Stub macros; depends only on libev otherwise. |
| **condensed-uuid** | `cuuid/` | UUID generation + variable-length condenser for v1 time-UUIDs | Mod | Med | Decouple the node-salt from `Node::get_local_node` (inject the node id). |

## Wave 4 — keystones & hard cases

High value, but each needs a good core peeled away from real domain glue.

| Proposed lib | Source | What it is | Diff | Value | Split plan |
|---|---|---|---|---|---|
| **htm-geo** | `geospatial/`, `cartesian.*` | Geometry → contiguous integer trixel ranges on a sphere (HTM), full shape support | Mod | High | Cut `THROW`/`strings::format`; drop the ~940 lines of matplotlib debug writers (`htm.cc:838+`). **Fix the `Cartesian` `operator^=`/`operator*` bugs first** (see ARCHITECTURE.md). |
| **haystack-store** | `storage.h` (+ lz4-stream) | Single-file append-only blob store templated on format structs | Mod | High | Replace `opts.*` with constructor params; stub logging. **Wire up the inert footer/header validation** before relying on it. |
| **iso8601 / datemath** | `datetime.*` (string/`tm_t`/`clk_t`/calendar core) | ISO-8601 + Elasticsearch-style date-math parser/formatter | Hard | High | Peel off every `const MsgPack&` overload + `phf`/`reserved` dispatch; vendor `strict_stox`; reparent exceptions onto `std::runtime_error`. The pure parser/formatter is excellent. |
| **sortable-serialise** ⚠️ | `sortable_serialise.*` | Order-preserving (`memcmp`-sortable) float/double encoding | Easy | High | Two files, `<cmath>`/`<cstring>` only — but **GPL** (Xapian origin). License-incompatible with the MIT libs; keep separate or reimplement. |
| **msgpack-patch** | `msgpack_patcher.*` | RFC 6902 JSON-Patch over a msgpack DOM | Mod | Med | Travels with a future **MsgPack** extraction; collapse `ClientError`/`Error` into one `patch_error`; keep RapidJSON only for the RFC 6901 tokenizer (or hand-roll it). |

> **The MsgPack keystone.** `msgpack.h` (the COW wrapper with the custom
> `UNDEFINED` ext type) is the hub several Wave-4 pieces hang off
> (`msgpack-patch`, and the data-interchange side of `cast`). It's a *Hard*
> extraction in its own right (entangled with `config.h`, exceptions, RapidJSON,
> ChaiScript), but if you ever lift it, the `msgpack-xchange` adapters and
> `msgpack-patch` become a natural family around it.

---

## Suggested bundles (the repos to actually create)

Many entries above are tiny and should ship together. A sensible repo set:

- **Foundational singletons:** `perfect-hash`, `lru-cache`, `callable-traits`,
  `errno-names`, `located-exception`, `bloom-filter`, `sortable-serialise` (GPL,
  separate).
- **`string-kit`** — `constexpr-strings` + `split` + `string-metrics` + `phonetic`
  (or split metrics/phonetic into their own repo if you want them findable by name).
- **`term-color`** — the ANSI/color trio.
- **`scheduler`** — `stash` + `thread-pool` + `timer-scheduler` + `debouncer` as
  one repo with internal layering (or `stash` standalone + `scheduler` depending
  on it). See SCHEDULER.md.
- **`sysutil`** — `posix-io` + `sysinfo` + `timeutil` + `alloc-kit` + `traceback`
  (the "systems-programming odds and ends" most projects re-implement).
- **`msgpack-x`** — `msgpack-xchange` now; `msgpack-patch` later if MsgPack is lifted.
- **Standalone heavies:** `htm-geo`, `haystack-store`, `libev-reactor`,
  `iso8601`, `urlcodec`, `mime-types`, `condensed-uuid`.

## Provenance & license map

Carry these notices on extraction; most of the tree is Dubalu LLC MIT.

| Source | Origin / license | Action |
|---|---|---|
| `sortable_serialise.*` | Xapian — **GPL** | Keep in a GPL-licensed repo, or reimplement clean-room. Do **not** mix into the MIT libs. |
| `allocators.h` (pool allocator) | moya-lang.org — **BSD-3** | Keep the BSD-3 notice in `alloc-kit`. |
| `times.cc` (macOS `clock_gettime`) | MM Weiss — **BSD-3** | Keep the BSD-3 notice in `timeutil`. |
| `xchange/rapidjson.hpp` | co-credited to xpol | Keep xpol's attribution. |
| `io.cc` (fallocate fallback) | SQLite — public domain | Fine; note it. |
| `y2j/` | **Mapzen — MIT (vendored)** | Not Xapiand's to republish; point users upstream. |
| `http_parser.h` | **Joyent/Node.js (vendored)** | Not ours; use upstream. |
| everything else | Dubalu LLC — MIT | Standard. |

## Not worth extracting

Domain glue or too-coupled / too-thin to package:

- **Schema & query glue:** `cast.*`, `script.*`, `reserved/*`, `query_dsl.*`,
  `aggregations/*`, `multivalue/*`, `database/*`, `serialise*` (Xapiand schema model).
- **Cluster/transport:** `server/discovery.*`, `server/*_protocol*`, `manager.*`,
  `node.*`, `endpoint.*` (a good *reference* Raft-over-UDP, but not a library).
- **Vendored or vendored-glue:** `metrics.*` (your metric catalog over upstream
  prometheus-cpp), `chaipp/*` + `module_*` (ChaiScript↔document binding),
  `exception_xapian.h`, `cmdoutput.h` (TCLAP + `Package`), `readable_revents.hh`
  (libev-only, tiny), `y2j/`, `http_parser.h`.
- **Superseded / trivial:** `fs.*` (mostly `std::filesystem` now; only
  `normalize_path` + `quarantine_files` are value-add), `concurrent_queue.h` /
  `atomic_shared_ptr.h` (thin shims; the former has a locking bug), `check_size.*`
  (debug-only), `PathParser` (bespoke URL dialect).

## Suggested sequencing

1. **Wave 1 first** — they're nearly free and several (errno-names, located-exception,
   the msgpack adapters, perfect-hash) are immediately useful elsewhere.
2. **`scheduler` next** — it's the highest-interest piece, and `stash` is the most
   novel thing in the whole codebase. SCHEDULER.md is already its blueprint.
3. **`string-kit` and `term-color`** — high value, all Wave-1/2 effort.
4. **`sysutil`** — bundles a lot of Wave-2/3 systems plumbing into one useful repo.
5. **The heavies last** — `htm-geo`, `haystack-store`, `iso8601` reward the effort
   but each needs real decoupling and a bug fix or two on the way out.

Before extracting any component flagged with a bug in ARCHITECTURE.md, fix the bug
on the way out — a freshly-published library is the worst place to ship a known
`operator^=` corruption or an inverted `empty()`.
