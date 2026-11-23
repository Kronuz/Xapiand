# split-storage: journal

State: waiting on job 261121-212343-084e42 (A3: ctest -N base vs head diff)
Quest: .campaign/xapiand/quests/split-storage.md (the lead's; never edit it)

## Doing

Split done and committed (storage:53d3255). Waiting on jobs; push done once.

## Next

1. When 261121-212343-084e42 passes (prints SAME), set State to "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-122326-e5a752 (the build-test check); job 261121-122326-e5a752 (build-test) |
| A2 | 261121-155804-60211a (split_check at 53d3255: 0 errors, 0 warnings, 1 note); build-test 261121-122326-e5a752 |
| A3 | 261121-212343-084e42 (base e0e7bed vs head, SPLITTING.md recipe); head list 261121-122336-a5af06 (11 tests) |
| A4 | docs/source-layout.md at storage:53d3255 |
| A5 | CI green at 53d325543622 |
| A6 | 261121-170219-d2f630 (resubmit; replaces failed 261121-134307-6878c9) |
| A7 | lead, 2026-11-21: read the split; storage_defs.h holds the hooks and definitions Storage<> builds on; opens with what it holds |

Failed jobs, superseded: split check 261121-122326-a913a0 failed on the old checker (it flagged every
line of docs/source-layout.md as an unallowed added line); rerun with the landed split_check.py,
261121-155804-60211a, passes. new-headers 261121-122339-5b6f5f (exit 125, empty log) hit the same
wrong-slot failure as below; 261121-170219-d2f630 passes (storage_defs.h compiles alone, c++20).

Split-check warnings: none. One note, storage.h has 1,373 lines (over 1000): kept whole on purpose,
one class template whose members share state (reason in docs/source-layout.md).

A6 failure (261121-134307-6878c9, exit 125, empty log): not a missing build step. Its slot was `sc`,
a worktree of the msgpack repo, so `git checkout 53d3255` there failed ("unable to read tree"): the
commit is in the storage repo only. Resubmitted without `--slot` (the quest's own slot,
xapiand-split-storage) with `--build "cmake -G Ninja -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"`.

## Result

storage.h (1,753 lines) -> storage.h (1,371: the Storage class template plus includes) +
storage_defs.h (425: hooks, format constants, codec dispatch, lock helpers, exceptions, header
structs). One slice (old lines 52-434) moved; the only added lines are storage_defs.h's license,
leading comment, `#pragma once` and includes, and storage.h's `#include "storage_defs.h"`.
No linkage or initializer changes (nothing lost `static`; the constants are `constexpr`/macros).
Layout note: docs/source-layout.md.

## Rulings I made

- Kept the Storage class template whole (~1,310 lines): one class, members share state and the
  v1/v2 paths split by `if constexpr` inside them; reason in docs/source-layout.md.
- One new header, not several: exceptions (~65 lines) and the codec helpers are too small alone,
  and the slice is contiguous, so storage_defs.h also carries the hook/IO-policy includes the
  header structs and lock helpers depend on (keeps it compiling alone).
- Tests (test_stream.cc 905 lines) are under 1,000: untouched.
