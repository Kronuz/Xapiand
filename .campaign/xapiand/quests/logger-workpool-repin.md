# logger-workpool-repin: Repin logger 6e7ab9e and fetch workpool by its name

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status logger-workpool-repin`.

## Outcome

Xapiand pins logger:6e7ab9e (Detent's burst-cost fix) and fetches workpool from `workpool.git` under the name `workpool`, at its main (at or after workpool:406d2f3), instead of `threadpool.git` at 63e03a5. No source behavior changes.

## Acceptance criteria

- A1 Pins: logger 6e7ab9e and workpool main as full shas; no `threadpool` name left in CMakeLists.txt; the full suite passes: the `build-test` check
- A2 The docs E2E passes: the `e2e` check
- A3 Logging cost: the logger burst row Detent measured (4 producers) is within its A/A band against f61d8df in Xapiand's build, or Detent's job is cited if it ran Xapiand's pin: a job

## Decisions

- Lead, 11-16: one repin quest for both, after serialize-repin (both touch CMakeLists.txt).

## May land early

None.
