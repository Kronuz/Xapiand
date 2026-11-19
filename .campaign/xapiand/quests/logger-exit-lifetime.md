# logger-exit-lifetime: logger's worker never touches destroyed statics at exit

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status logger-exit-lifetime`.

## Outcome

The logger's worker thread, which still drains and emits lines while `LogWorker::close` runs during process exit, never reaches a static that exit has already destroyed. Found by coverage-areas (job 261118-163254-b09a49): under a single LLVM 22 libc++, Xapiand's aggregations test aborts 20 of 20 at exit with `mutex lock failed: Invalid argument`. The stack is `LogWorker::close` -> `pass` -> `fire_head` -> `write_line` -> `Logging::emit` (logger.cc:1767) -> `OnceFilter::seen_or_add` (logger.cc:304), locking the mutex of `static OnceFilter filter` (logger.cc:1765) after its destructor ran. Apple's libc++ tolerates locking a destroyed mutex; LLVM's does not.

## Acceptance criteria

- A1 [artifact] Every static that `Logging::emit` and the worker's drain path reach is listed with why it is safe at exit, and the fix makes each either never destroyed or out of reach once exit begins (reordering definitions is not a fix): `docs/` note in the logger repository
- A2 A test reproduces the abort without the fix (a process whose worker emits a `once` line during exit aborts, or ASan reports use-after-destroy) at the parent commit and passes at the fix: a `--fails-without` job
- A3 ASan and TSan builds of the logger suite pass: jobs
- A4 The suite passes and CI is green on every platform

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: high risk (object lifetime across threads at exit). Xapiand repins logger in coverage-areas, whose aggregations test must then pass 20 of 20 under one libc++. Logger is shared with Detent: tell its lead when this lands.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
