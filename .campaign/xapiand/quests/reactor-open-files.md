# reactor-open-files: raise_open_files clamps, and its untested paths get tests

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state is
in `state.json`: `campaign status reactor-open-files`.

## Outcome

reactor review F3/F5/F6: `raise_open_files(SIZE_MAX)` casts to a negative `rlim_t` on FreeBSD and
raises nothing (clamp to `RLIM_INFINITY`/the hard limit); the macOS `maxfilesperproc` cap and the
refused-`setrlimit` paths get tests; `seconds_to_expiry` stops returning 0 for both an unreadable
certificate and one expiring now (distinct results).

## Acceptance criteria

- A1 `raise_open_files(SIZE_MAX)` raises to the hard limit on every platform: a test with values
  and a `--fails-without` job at the unclamped cast
- A2 The macOS cap and the refused-`setrlimit` paths each have a test with values
- A3 `seconds_to_expiry` tells unreadable from expiring-now: a test with values
- A4 [ci] CI is green on every platform (FreeBSD included)

## Contracts

Provides reactor:limits; any signature change is listed in the journal with its callers (Xapiand).

## May land early

Nothing: one landing.
