# Transport security, step 0: the dependency move

The gate evidence for step 0 (`docs/design/transport-security.md`, T0): every Kronuz library
moves from its 2026-09-18 pin to its current head, the 14 transitive pins go, and OpenSSL 3
joins the build behind `TLS`. This file maps each behavior-changing commit range to the check
that covers it (condition C1), and records the results as they come in.

## What moves

58 of 59 Kronuz libraries, 402 commits. Of those, 119 change code (one of them, in scheduler, is a docs commit that also touches a header); the rest are CI, docs,
tests, benchmarks, and pin commits that touch only `CMakeLists.txt`. Listed per library,
the code changes since each 2026-09-18 pin:

| Library | Code changes | What they change | Covered by |
|---|---|---|---|
| cluster | `raft.h`: affirmative voting majority (`7c7dd86`), ballots kept for the whole term (`6f6fcc3`), commit only after verified replication (`23407e8`), messages validated before their terms are observed (`5b2a141`), inherited entries kept on append (`1697e56`), elections stop at term exhaustion (`510e120`); `length.h` rejects integer overflow (`5f583e5`) and depends on varint (`fd5c043`); `liveness.h` added (`5e6feee`); the rest moves completion, journal, consensus, and metrics out of the repository (`e70b5fc`), which Xapiand does not include | Leader election and commitment, including a two-node cluster's ability to elect with one node down | `harness/cluster_check.sh` with 2 and 3 nodes; a leader kill with 3 nodes (failover); `wal_recovery_check.sh` |
| reactor | Accept recovery under descriptor pressure (`9016fdb`), connection admission before dispatch (`0759c9a`, `a0bc26c`), listener readiness and startup failures (`6f68823`), sessions and accepts reaped before stop (`863bed0`, `e6edfc4`), stale timers not rearmed after stop (`3ba7637`), TLS (`d5bd39b`, `56783e5`) | Startup, accept, and shutdown of every TCP service | smoke, load, e2e; `graceful_shutdown_check.sh`; `signal_check.sh` |
| http | Request deadlines and connection bounds (`8ff1841`, `9e3478c`), owned and deferred replies (`c3193b9`, `af36648`, `8f27b3c`), streaming handlers joined before storage is freed (`70cf14f`), worker admission (`2a422ce`, `29d62a7`), TLS sessions (`708f457`), `Request::remote` (`cf84df3`) | Every REST request; unset deadlines and bounds stay off | e2e (570 assertions), smoke, load |
| threadpool | `join()` waits for the promise (`8fad6fa`), `TaskGroup` (`fdc0a1e`), defined restart (`628679a`) | Shutdown of every thread Xapiand starts | unit tests; `graceful_shutdown_check.sh`; `signal_check.sh` |
| scheduler | The debouncer keeps the latest arguments at its force deadline (`d0ea8df`); the random library replaces a vendored copy (`3ec5d7d`) | Debounced commits and replication triggers | unit tests; `cluster_check.sh` (replication) |
| cuuid | v1 ids distinct after compacting (`0163d7c`), v6 generated monotonically (`a7dd5e3`) | Document ids | unit tests; e2e |
| msgpack | Integer equality across types, copied strings (`d094091`) | Every document and query value | unit tests; e2e |
| hashes | xxHash inline, CRC-32C (`abff6fa`, `90ec405`) | Hash values used in dispatch and storage | unit tests; e2e; `wal_recovery_check.sh` (storage checksums) |
| md5, sha256 | Unaligned word loads and in-bounds padding (`27dd372`, `4febffc`, `3476a81`, `8acc96b`) | Digests, now defined behavior | unit tests |
| compressors | Bounded zstd decompression, per-backend options (`7a6cc22`) | Stored and replicated data | e2e; `wal_recovery_check.sh`; `cluster_check.sh` (replication) |
| boolean-parser | The AST built without recursion; operand order fixed (`ac248d1`) | Query parsing | e2e |
| cartesian | `^=` fixed; scaling returns a copy (`34bf069`) | Geospatial math | e2e (geo pages); unit tests |
| soundex | German and Spanish rules in a defined order (`271e129`) | Phonetic matching | unit tests |
| lazy | Holds the invocable by value (`49e5245`) | Lifetime of deferred values | unit tests; ASan |
| logger, term-color | The opt-in tint, `name_color`, `gradient_banner` (`5bd491c`, `9abe858`) | Nothing unless used | build |
| system | FreeBSD process memory, disk, inode, and file limits (`7ca9a38`) | FreeBSD only | the FreeBSD package build (C3) |
| split, stash, traceback | Missing includes; traceback's opt-in hooks (`5f2ca8a`, `5750708`, `e2fd792`) | Build only | build |

## Pin consistency (C2)

After the move, every library's consumers must pin the same commit of it. A consistency pass
moves lagging pins in the libraries themselves (hashes, termdict, analysis, residency,
completion) and re-pins their consumers; journal and consensus are left for Detent's
coherent upgrade, and Xapiand includes neither.

## Results

| Check | Linux | macOS |
|---|---|---|
| Configure: every Kronuz library checked out at its pinned commit | | pass |
| `pin_consistency.py`: one commit per library across consumers | pass (host-independent): only journal and consensus, which Xapiand does not include, disagree | |
| Build, unit tests | | pass |
| smoke, load | | pass |
| e2e | | pass (newman 6) |
| `cluster_check.sh`, 2 nodes | | pass |
| `cluster_check.sh`, 3 nodes | | pass |
| Leader kill, 3 nodes (`FAILOVER=1 cluster_check.sh 3`) | | pass: a new leader; the killed node rejoins; one leader; writes resume |
| `wal_recovery_check.sh` | | pass |
| `graceful_shutdown_check.sh` | | pass |
| `signal_check.sh` | | pass |
| Docker image, FreeBSD package, Homebrew bottle, without publishing; the OpenSSL each links (C3) | | |

macOS (Darwin 24.6, Apple clang, Homebrew OpenSSL 3), at the step-0 tree, 2026-10-17, in a fresh
build directory without the local override.

**The leader kill.** With `--primary-node node1`, node1 holds the cluster database's primary,
which takes every write; killing it makes writes return 503 ("Primary node node1 is not
active!") until it returns, by design, at the 2026-09-18 pins as at the new ones. The check
therefore asserts what the Raft changes govern: the survivors elect one new leader, the
killed node rejoins, one leader remains, and writes through a survivor read back from the
rejoined node. Both the step-0 build and the previous pins pass it.
