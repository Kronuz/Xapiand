# split-msgpack: journal

State: waiting on jobs 261121-145023-9444f2 (A2 split_check, fixed checker, at 68e45d8; main already merged)
Quest: .campaign/xapiand/quests/split-msgpack.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reassessed against cpp-libraries docs/SPLITTING.md; pushed at msgpack:68e45d8 (a43dc0f still tracked the five superseded pieces, so A6 261121-121108-797630 failed on redefinitions; 68e45d8 deletes them, and the tree now holds exactly the ten pieces plus msgpack.h). msgpack.h is now an umbrella over ten self-contained pieces (82 to 848 lines): types, class (class, Iterator, Body, adaptors), ctor, init, modify (find/erase/put/fill and the public modifiers), access, hash, convert, arithmetic, serialize. test/diff_test.cc is kept whole again (the earlier split is reverted). docs/source-layout.md lists the pieces and why diff_test.cc stays whole.

## Next

1. Read the jobs below.
2. Answer the warnings of 261121-145023-9444f2 (using-declarations, dropped static, new prototypes); fix any error with moves only and rerun.

## Blocked on

Nothing (split waits on split-check landing).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-121649-8e64c6 |
| A2 | 261121-145023-9444f2 (fixed split_check from split-check-sources, at 68e45d8; replaces 261121-123003-6b4772) |
| A3 | ctest -N before 261121-115120-acd826 (1e9aaaf4ac5b), after 261121-121651-f87cf4 at 68e45d8 (the earlier after-run at edfd97b listed the same 13 tests) |
| A4 | docs/source-layout.md |
| A5 | CI on the pushed branch |
| A6 | selfcontained.sh job 261121-121647-167eff at 68e45d8 (261121-121108-797630 at a43dc0f failed: superseded pieces still tracked) (earlier runs: 261121-115616-071a12 failed on msgpack_ctor.h, 261121-120211-04c861 passed on the 16-header layout) |
| A7 | lead, 2026-11-21: read the split; msgpack.h cut by member family (class body whole in msgpack_class.h); msgpack_types.h is 82 lines but is the constants and enum header every piece builds on, accepted as a unit |

## Result

Pending the jobs.

## Rulings I made

- Ten pieces, merged by what a maintainer works on together: the class declaration with its nested Iterator and Body and its msgpack adaptors (one type and its adaptors); the internal find/erase/put/fill with the public modifiers (one family); the rest stay by responsibility (ctor, init, access, hash, convert, arithmetic, serialize, all over 100 lines). msgpack_types.h (82 lines) stays as the base every piece includes. msgpack_exception.h (35 lines) is a pre-existing file, not touched.
- Pieces are self-contained: the external includes move (unchanged) from msgpack.h to msgpack_types.h; every other piece includes msgpack_class.h and the standard headers it uses. msgpack.h only includes the pieces.
- The msgpack adaptor specializations moved, unchanged, from the end of the file to msgpack_class.h, ahead of the constructors: msgpack_ctor.h instantiates msgpack::object for MsgPack and failed alone without them. Their position in the file order is the one non-move.
- `static const char* undefined = "\0";` moved with the constants into msgpack_types.h: a constant-initialized object, no dynamic initializer.
- test/diff_test.cc kept whole (1,026 lines): one differential test whose generator, runner and checks share one anonymous namespace in one translation unit; the three-header split put fragments inside it, so it was reverted rather than turned into separate translation units (that needs a linkage change for no gain). Earlier commits e64b234 and its revert 84bbc99 are in the branch history.
- No CMake change: no install rules or header list exist (include directory only); ctest names unchanged.

- Lead, 2026-11-21: split checks 261121-121649-b844e7, 261121-155625-47439d and 261121-155800-2547bb failed on the checker before split-tooling (macro-call and macro-namespace shapes, repeated lines); the landed checker passes at the same commit (261121-192206-226c8f).
