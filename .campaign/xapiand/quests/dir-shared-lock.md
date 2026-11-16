# dir-shared-lock: completion's IO takes a shared directory lock beside the exclusive owner lock

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status dir-shared-lock`.

## Outcome

completion's `IO` offers a shared directory lock beside the exclusive `acquire_directory_owner()` (io.h:195): `acquire_directory_reader()` (name the contributor's call) returns an `OwnerLock` holding `flock(LOCK_SH|LOCK_NB)` on the directory in `PosixIO`, with the exclusive lock's busy and not-supported rules; a virtual with a default like the owner lock's, so existing IO wrappers in Xapiand, strata and Detent keep compiling. Many shared holders coexist; a shared holder makes the exclusive lock busy and the reverse. strata's read-only-open (D60a) is its first user.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 Shared holders coexist across separate opens; shared against exclusive and exclusive against shared are busy both ways; releasing the last shared holder frees the exclusive lock; tests in the suite, with a `--fails-without` job (shared mapped to exclusive)
- A2 The default implementation and the not-supported path behave like `acquire_directory_owner()`'s, shown by tests
- A3 [artifact] The lock contract, including that an SH-to-EX conversion is two steps the caller owns, in `docs/mechanisms/`: the note's path
- A4 CI green on every platform: the build-test job

## Contracts

Provides completion:directory-lock: `IO::acquire_directory_reader()` (or the chosen name) returning `std::unique_ptr<OwnerLock>`, a virtual with a default; Posix uses flock(LOCK_SH|LOCK_NB). Detent consumes completion and is told when it lands.

## Decisions

- Owner, 11-12 (D60a): read-only opens take a shared flock; this quest supplies it.

## May land early

None (shared public API).
