# split-msgpack: journal

State: waiting on jobs 261121-115108-d73489 261121-115114-0ef6f8 261121-115120-acd826 (split check needs split-check landed)
Quest: .campaign/xapiand/quests/split-msgpack.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed (quest/xapiand/split-msgpack, msgpack:6bd3c5b): msgpack.h into 14 msgpack_*.h pieces (52 to 449 lines), test/diff_test.cc into diff_test_{gen,run,check}.h (266 to 360) plus a 121-line diff_test.cc, docs/source-layout.md. Nonblank lines checked equal to the original in order. No install rules or header list exist in CMakeLists.txt (include dir only), so nothing to add there.

## Next

1. Read build-test and the two ctest -N jobs (before 261121-115120-acd826 at 1e9aaaf4ac5b, after 261121-115114-0ef6f8); diff their listings.
2. Rerun `campaign verify split-msgpack` for split once split-check has landed (job 261121-115108-bae441 failed exit 2 because tools/split_check.py does not exist yet); answer its warnings below.

## Blocked on

Nothing (split waits on split-check landing).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-115108-d73489 |
| A2 | split job 261121-115108-bae441 failed exit 2 (tool not landed); rerun after split-check |
| A3 | ctest -N jobs 261121-115120-acd826 (before) and 261121-115114-0ef6f8 (after), diffed |
| A4 | docs/source-layout.md lists every new file and what it holds |
| A5 | CI on the pushed branch |

## Result

Pending the jobs.

## Rulings I made

- msgpack.h keeps the license and the external includes (lines 1-55 unchanged) and then includes the pieces in the original order; the pieces carry no includes of their own and are not self-contained (documented in docs/source-layout.md).
- Each piece opens with the license, a one-line comment and `#pragma once`: the only added lines. Blank runs between pieces are dropped.
- `static const char* undefined = "\0";` moved with the constants into msgpack_types.h: a constant-initialized object, no dynamic initializer; it stays one per translation unit as before.
- test/diff_test.cc includes its three pieces inside its one `namespace { }`, so the pieces hold only code and no namespace lines. No shared helper was introduced.
- No CMake change: the msgpack_diff_test target still builds the single test/diff_test.cc; ctest names are unchanged.
