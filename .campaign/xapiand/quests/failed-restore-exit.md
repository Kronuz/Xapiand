# failed-restore-exit: A CLI restore of a file that is not a dump fails

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status failed-restore-exit`.

## Outcome

`xapiand --restore FILE` exits EX_DATAERR (65) with "Restore object has an unsupported type: X (the
file is not a dump)" when FILE holds anything but document maps, and creates no index. Before, any
text file parsed as a stream of small msgpack integers, DocIndexer skipped each with a log line, and
the CLI exited 0, so scripts saw success. A valid dump restores the same documents and exits 0. The
HTTP RESTORE path is unchanged. From the coverage-core-main job 261119-044115-72c517 (hunt list).

## Acceptance criteria

- A1 A non-dump file and a dump with a stray integer both exit 65, and a valid dump restores and
  re-dumps to the same sha256 with exit 0: `harness/restore_exit_check.sh`, passing at the fix and
  failing at Xapiand:586374bf7, the script alone (`--fails-without`)
- A2 The suite passes at the landed code: a job

## Contracts

None: only the CLI's exit status for input that is not a dump changes, from 0 to 65.

## Decisions

- Any other ClientError in the CLI restore now exits 65 instead of 70 (EX_SOFTWARE): bad input is a
  data error.
- A truncated dump still exits 0; that is the hunt row "A truncated dump restores what it holds and
  exits 0".

## May land early

None.

## Tasks

None: the fix and its check are Xapiand:586374bf7 and Xapiand:9d21bdd2d on
explore/xapiand/failed-restore-exit (explorer jobs 261121-052406-9ae8c0, failing at the script
alone, and 261121-052447-be339d, passing at the fix).
