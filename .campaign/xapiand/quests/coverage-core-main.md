# coverage-core-main: main.cc and manager.cc reach T13 coverage through E2E drivers

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status coverage-core-main`.

## Outcome

main.cc and manager.cc reach T13's target (about 590 uncovered lines) through E2E drivers run in the instrumented build.

## Acceptance criteria

- A1 Line and branch coverage of main.cc and manager.cc at or above the measured floor: main.cc 81% lines / 63% branches, manager.cc 79% / 52% (harness/coverage.sh before and after). Lowered 2026-11-19 from T13's 90/80 (lead): the remaining lines need root, a tty, a failing setuid, a never-ending write or death by signal (Evidence lists the ranges), and manager.cc is replaced by the server-composition adoption (D74, D132), so no fault-injection hooks are added to it; hunt row filed
- A2 Every new test checks values, not only that code ran; a test of a fixed bug fails without the fix
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: split from coverage-areas (its journal, "Proposed split"); floors stay at T13, never reset as a ratchet.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
