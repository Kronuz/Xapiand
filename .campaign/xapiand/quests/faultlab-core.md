# faultlab-core: Create Kronuz/faultlab: faultfs, recorder, scenario, events, checker

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-core`.

## Outcome

Kronuz/faultlab exists (public MIT, D95), with migration step 1 of cpp-libraries `docs/design/faultlab.md` (cpp-libraries:727e417): faultfs.h from strata's core/faultfs.hh (784 lines) with F2-F7; recorder.h (F10, ~200 lines); scenario.h and events.h (F11 and its event log); and the Python half: history.py (365, with `monotonic` and `seal`), selftest.py (446), proxy.py (263) and proxy_test.py (99) from Xapiand's harness/cluster. CI runs on Linux, macOS and FreeBSD. No consumer moves in this quest (steps 2-10 are their own quests).

## Acceptance criteria

- A1 F5: each fixed pick's state equals `crash` with that field's coins forced, checked directly: a test.
- A2 F3: a recorded table of strata seeds and the states they rebuild, unchanged from strata's faultfs: a test that fails when one seed's state changes.
- A3 `size_pick::sector`: every MemoryIO and CrashIO state over random append sequences is a faultlab state (the design review's probe, Xapiand `.scratch/review-faultlab-design-design/sizes.cc`, as a test).
- A4 `link`, `try_lease` and `map_shared` crash rules exactly as the design's build-step checks list them: tests.
- A5 The recorder's `verify()` catches a store whose real bytes differ from the shadow, and the Python checker and proxy pass their own suites in CI: tests.
- A6 [artifact] `README.md` and `docs/` describe the API; CI is green on Linux, macOS and FreeBSD.

## Contracts

Provides `faultlab:faultfs` (faultfs.h, recorder.h, scenario.h, events.h) over `completion:io` with F8's additions. Consumers adopt it in later quests: prospect, strata, Xapiand, journal, consensus, Detent (Detent's own, D104), storage, io.

## Decisions

Owner D95: header-only, portable; Linux labs optional and later. The GitHub repository is created and pushed when this quest lands (D95 names it; the lead creates it). F9 pending the owner; faultfs models only the durable contract.

## May land early

Headers behind tests may land early; nothing changes for consumers until they repin.
