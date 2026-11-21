# freebsd-exe-path: journal

State: ready for review
Quest: .campaign/xapiand/quests/freebsd-exe-path.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Link fix (opts renamed) at 53b316116; first jobs failed duplicate _opts. Implemented at b6d1dc4b2: FreeBSD KERN_PROC_PATHNAME branch in src/main.cc get_executable_path; tests/test_executable_path.cc (includes main.cc, calls get_executable_path(nullptr), compares to the binary name and to a call with a bogus argv0). Jobs queued: A1 targeted build+run 261119-144302-05db76; check suite 261119-144308-997915.

## Next

1. Review; A1's FreeBSD half and A3's CI come from CI at 9f8b95f74.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | job 261119-151950-269667 passed (macOS); FreeBSD half on CI at 9f8b95f74 |
| A2 | noted: workaround lives only on quest/xapiand/coverage-core-main, outside this file set (Rulings) |
| A3 | suite job 261119-151953-11ac9b passed; CI green pending at 9f8b95f74 |

## Result

get_executable_path asks the kernel on FreeBSD (KERN_PROC_PATHNAME); tests/test_executable_path.cc checks it directly. Open: FreeBSD CI.

## Rulings I made

- A2: the workaround (e0ad77c2c, tests/test_coverage_main.cc test_data_path_beside_the_executable) exists only on quest/xapiand/coverage-core-main, not on this branch and outside this quest's file set. Not edited. Once that quest lands, its test should call get_executable_path(nullptr) directly and drop the FreeBSD sysctl block.
- Test includes ../src/main.cc with main renamed, as test_coverage_main does; no CMake change needed (tests glob).
