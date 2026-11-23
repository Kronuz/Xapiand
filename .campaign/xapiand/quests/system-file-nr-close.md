# system-file-nr-close: Linux file-nr readers close their descriptor

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status system-file-nr-close`.

## Outcome

On Linux, `get_open_files_system_wide` and `get_max_files_system_wide` each open
`/proc/sys/fs/file-nr` and never close it, on every path, and parse a buffer `read` never
terminates. Xapiand's `adjustOpenFilesLimit` calls both, so every start holds two stray
descriptors, and open-files-floor's "start at the exact minimum" case fails on every Linux CI job
(run 44966941683 at Xapiand:4a07d2e: it counts 3 open, the function 5). After this quest one
reader opens the file, reads a terminated line and closes the descriptor on every path, and both
functions use it.

## Acceptance criteria

- A1 Calling each system-wide reader leaves the process's open-descriptor count unchanged: a check
  in test/test.cc, run by the `build-test` check
- A2 [ci] The same check fails on Linux CI with the close removed (a mutant commit pushed to a
  `quest/**` branch, its failing run cited), and CI is green on every platform at the fix

## Contracts

None changes: the functions keep their signatures and what they measure.

## Decisions

- Lead, 2026-11-23: one helper reads file-nr for both functions, so the open, terminate and close
  are written once. Review waived for a small fix whose test fails without it on the platform it
  fixes.

## May land early

Nothing: one commit.

## Tasks

None: the lead.
