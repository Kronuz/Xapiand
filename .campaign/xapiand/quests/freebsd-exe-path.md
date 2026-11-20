# freebsd-exe-path: FreeBSD: get_executable_path finds the executable

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status freebsd-exe-path`.

## Outcome

`get_executable_path` returns the executable's path on FreeBSD (sysctl KERN_PROC_PATHNAME), so files found beside the executable are found there too. Today src/main.cc has no FreeBSD source and returns empty.

## Acceptance criteria

- A1 A new test checks get_executable_path directly against the real binary path on every platform; it fails on FreeBSD without the fix (CI row)
- A2 [manual] test_coverage_main's workaround (e0ad77c2c) resolving the path itself is removed once A1 holds, if it lies in this quest's files; otherwise noted in the journal and filed in hunt.md: the lead checks the note
- A3 The suite passes and CI is green on all platforms

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
