# require-once: journal

State: waiting on jobs 261122-230814-920115 261122-230842-8fa903
Quest: .campaign/xapiand/quests/require-once.md (the lead's; never edit it)

## Doing

REQUIRE and REQUIRE_EQ rewritten (tests/test_harness.h, xapiand:b6e852eba): each counts one check,
evaluates its condition (each operand) once into a local, prints its own FAIL line naming REQUIRE
and returns. Self-check test_require_evaluates_once in tests/test_string.cc: counters read 1 per
macro, a failing REQUIRE and REQUIRE_EQ each print one FAIL line, return, and add one failure
(the test removes that intended failure after counting it, with stderr captured). Local sanity at
the old macros: 5 of its checks fail; at the new: all pass.

## Next

1. Read the suite job; if newly live checks fail, name each test and cause here (A2).
2. Read the mutant job log (BUILD COMPLETE, then named checks failing) for A1.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite 261122-230814-920115 at b6e852eba (runs test_string); fails-without 261122-230842-8fa903 at mutant/xapiand/require-once ee5133bbf (pushed): a plain `c++` build of test_string.cc at the old macros, then it runs; must show BUILD COMPLETE and test_string.cc test_require_evaluates_once checks failing |
| A2 | suite 261122-230814-920115 |
| A3 | manual, below |

A3: every REQUIRE/REQUIRE_EQ in tests/ (134 lines, excluding test_string.cc), read by hand. Pure
conditions (size(), nullptr, empty(), find, count, ok(), finished, ready(), fd >= 0, merges_pending(),
merge_output_unsynced(), capacity(), docs_in()) are not listed. Conditions that call something with an
effect, and whether a second run differed:

| file:line | condition | second run differs? |
|---|---|---|
| tests/test_coverage_main.cc:304, 313, 357 | `::getcwd(buf, n) != nullptr` writes buf | no: same result, same buffer |
| tests/test_coverage_main.cc:359 | `usedir_code(root + "/data", false) == -1` (mkdir, chdir) | no: the path is absolute and mkdir of an existing directory succeeds, so a second run returns -1 again |
| tests/test_lua.cc:314 | `call(processor, MAP()).ok()` runs a Lua call (36 MiB of strings) | not in the passing run: it was true twice; the second call only costs time and memory |
| tests/test_lua.cc:408 (in a 2-iteration loop) | `call(processor, MAP()).ok()` then `CHECK_LE(resident_bytes, idle + 16K)` | not in the passing run: 4 calls ran, now 2; the resident-bytes check follows each |
| tests/test_pool.cc:2139 | `until([&]{ return opening.load(); })` polls up to 5 s, then re-polls | a second run could only differ by racing; it waits again, never silently skipped |
| tests/test_single_flight.cc:112, 115, 144, 165, 170 | `eventually(...)` polls up to 3 s, then re-polls | same: a poll, a failing first run printed FAIL, a second run that passed only continued |
| tests/test_text.cc:186 | `row_config(r, c)` resets c and parses r (`r["flags"]` inserts a key) | no: it resets c each call and the inserted key is empty-then-same |
| tests/test_lua.cc:330 and the others `p->outcome.ok()`, `node.ready()` | getters | n/a |

The one known case (tests/test_restore_batch.cc:246) is on journal-replaces-wal's restore branch and
is not touched here. The suite may show a change in tests/test_lua.cc:408 (call count 4 to 2).

## Result

Pending the suite and the mutant job.

## Rulings I made

- The self-check captures stderr with dup2 to a tmpfile (POSIX, as the tests already are) so the
  intended FAIL lines of the failing REQUIRE cases stay out of the suite log, and removes the
  intended failures from testh::failures after counting them.
- The mutant job builds test_string.cc directly with `c++ -std=c++17 -Itests` (the test is
  header-only apart from the harness) instead of configuring the whole tree: a complete build, then
  the named checks failing.
