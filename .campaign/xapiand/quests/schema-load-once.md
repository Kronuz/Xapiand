# schema-load-once: Concurrent first uses of a schema load it once, foreign links included

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status schema-load-once`.

## Outcome

Concurrent first uses of a schema load it once, and the foreign schema its metadata links to once, however the waiters wake. Today test_node.cc:535 (`test_concurrent_first_uses_of_a_schema_load_it_once`) saw 3 loads, not 2, under ASan+UBSan (CI run 43287306312); the held-turn check (1 load for 6 calls) passed, so the extra load comes after release, likely the foreign link loaded by more than one woken waiter. See hunt.md, "Duplicate schema load".

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The second loader is found and named in the journal with the interleaving that produces it, and a deterministic test (no sleeps or timing luck) forces that interleaving and counts exactly 2 loads: a test that fails without the fix (`--fails-without` job, exits 0 only when the named CHECK fails, 2 on a build error)
- A2 test_node passes 20 repeats under ASan+UBSan: a light job looping `ctest -R test_node`
- A3 The suite passes: the build-test check

## Contracts

None; no API change.

## Decisions

- Lead, 11-12: fix the dedup where the second load starts; no retry or tolerance in the test.

## May land early

None.
