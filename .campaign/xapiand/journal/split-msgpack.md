# split-msgpack: journal

State: waiting on jobs 261121-120214-36ce9b (build-test) 261121-120214-867ec4 (split: fails until split-check lands)
Quest: .campaign/xapiand/quests/split-msgpack.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed (msgpack:f107dd3). msgpack.h into 15 self-contained msgpack_*.h pieces (adaptor added so msgpack_ctor.h compiles alone), test/diff_test.cc into diff_test_{gen,run,check}.h plus a 121-line diff_test.cc, docs/source-layout.md. Nonblank lines of the originals equal the pieces in order (checked before the include and adaptor edits).

## Next

1. Read build-test 261121-120214-36ce9b.
2. Rerun `campaign verify split-msgpack` for split once split-check has landed (job 261121-120214-867ec4 fails exit 2 until tools/split_check.py exists); answer its warnings.

## Blocked on

Nothing (split waits on split-check landing).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-120214-36ce9b (superseded 261121-115108-d73489) |
| A2 | split job 261121-120214-867ec4 failed exit 2 earlier (tool not landed); rerun after split-check |
| A3 | ctest -N before 261121-115120-acd826 (1e9aaaf4ac5b) and after 261121-115625-2186ed (edfd97b): 13 tests, names identical; rerun at head 261121-120216-d9be17 |
| A4 | docs/source-layout.md lists every new file and what it holds |
| A5 | CI on the pushed branch |
| A6 | selfcontained.sh job 261121-120211-04c861 at f107dd3: 16 ok, 0 failed (new headers and msgpack.h, c++20). Earlier 261121-115616-071a12 failed on msgpack_ctor.h, fixed by msgpack_adaptor.h |

## Result

Pending the jobs.

## Rulings I made

- Pieces are self-contained: the external includes move (unchanged) from msgpack.h to msgpack_types.h, the base every piece includes; msgpack.h only includes the pieces in the original order.
- The msgpack adaptor specializations (convert/pack/object of MsgPack) moved, unchanged, from the end of msgpack_serialize.h to a new msgpack_adaptor.h included by msgpack_ctor.h and msgpack.h: msgpack_ctor.h instantiates msgpack::object for MsgPack and failed alone without them. They now precede the constructors instead of following everything.
- Each piece opens with the license, a one-line comment and `#pragma once`: the only added lines. Blank runs between pieces are dropped.
- `static const char* undefined = "\0";` moved with the constants into msgpack_types.h: a constant-initialized object, no dynamic initializer; it stays one per translation unit as before.
- test/diff_test.cc includes its three pieces inside its one `namespace { }`, so the pieces hold only code and no namespace lines. No shared helper was introduced.
- No CMake change: the msgpack_diff_test target still builds the single test/diff_test.cc; ctest names are unchanged.
