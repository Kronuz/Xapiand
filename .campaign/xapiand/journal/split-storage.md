# split-storage: journal

State: waiting on jobs 261121-122326-e5a752 261121-122326-a913a0 261121-122336-a5af06 261121-122337-4eb641 261121-122339-5b6f5f
Quest: .campaign/xapiand/quests/split-storage.md (the lead's; never edit it)

## Doing

Split done and committed (storage:53d3255). Waiting on jobs; push done once.

## Next

1. Read job results; diff the two `ctest -N` outputs (A3); fold in the split check's warnings (A2) once split_check.py lands (job exits 2 until then).

## Blocked on

Nothing (A2 waits on quest split-check landing tools/split_check.py).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-122326-e5a752 (build-test) |
| A2 | job 261121-122326-a913a0 (split; exit 2 until split_check.py lands) |
| A3 | head 261121-122336-a5af06, base 261121-122337-4eb641: diff the two outputs |
| A4 | docs/source-layout.md at storage:53d3255 |
| A5 | CI on the pushed branch |
| A6 | job 261121-122339-5b6f5f (selfcontained, storage_defs.h only: old headers skipped with -x) |
| A7 | lead, 2026-11-21: read the split; storage_defs.h holds the hooks and definitions Storage<> builds on; opens with what it holds |

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
