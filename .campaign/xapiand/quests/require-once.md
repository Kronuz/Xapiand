# require-once: REQUIRE evaluates its condition once

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status require-once`.

## Outcome

`REQUIRE(cond)` and `REQUIRE_EQ(a, b)` in `tests/test_harness.h` expand `CHECK(cond)` and then
`if (!(cond)) return;`, so the condition runs twice. When it has a side effect and the second run
differs from the first, the test function returns with no FAIL line and every later check in it is
dead. journal-replaces-wal's restore task found one (test_restore_batch.cc:246: an apply inside
REQUIRE applied twice, so the stored `_version` check never ran and the restamp mutant survived).
After this quest each REQUIRE evaluates its condition, and each operand of REQUIRE_EQ, exactly once,
prints its own FAIL line when it fails, and counts as one check, and the whole suite still passes.

## Acceptance criteria

- A1 A REQUIRE whose condition has a side effect runs it once, and a failing REQUIRE prints a FAIL
  line and returns: a self-check in tests/test_string.cc (a counter incremented inside REQUIRE and
  REQUIRE_EQ, read after), run by the `suite` check, with a fails-without job at the old macros
- A2 The whole suite passes with the new macros: the `suite` check. Any test whose newly live checks
  fail is named in the journal with the cause; a fix outside this file set goes under "Blocked on"
- A3 [manual] No other REQUIRE in tests/ relies on evaluating twice: the contributor lists each
  REQUIRE with a side-effecting condition it found (call, increment, assignment), and the lead reads it

## Contracts

None changes: the macros keep their names, arguments and the meaning "abort the test function".

## Decisions

- Lead, 2026-11-23: the self-check sits in tests/test_string.cc, a pure test no quest holds, because
  a new test file needs the root CMakeLists.txt, which cluster and open-files-floor hold.
- Lead, 2026-11-23: low risk; the lead reads the diff. Evaluate into a local (`const bool ok_ =
  static_cast<bool>(cond)`, and `const auto& a_ = (a); const auto& b_ = (b);` for REQUIRE_EQ, which
  extends temporaries), as CHECK_OP already evaluates once.

## May land early

Nothing: one commit.

## Tasks

None.
