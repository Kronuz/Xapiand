# catalog-stress-cap: catalog_stress reaches its maps cap deterministically

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state is
in `state.json`: `campaign status catalog-stress-cap`.

## Outcome

strata's `test/catalog_stress_test.cc:1013` `CHECK(met)` ("the maps cap met at its hard value")
misses in all 8 tries of `stress_two(defer_io)` on a slow runner (peaks 15 of 16 maps; strata
runs 44835046897 at e4561f685 and 44858073442 at b4694757). The test holds maps open until the cap
is reached instead of hoping a timed run touches it, and still checks that the cap holds.

## Acceptance criteria

- A1 The stress test passes 50 runs under a light hold beside other work, and a run slowed on
  purpose (a sleep in the workers) still meets the cap: a job for each
- A2 A mutant that lets the maps exceed the cap still fails the test (`--fails-without`)
- A3 [ci] CI is green on every platform, macos-x86_64 included

## Contracts

Test only.

## May land early

Nothing: one landing.
