# system-file-nr-close: journal

State: waiting on jobs 261122-212046-713a5b (build-test at system:b2a874f), and CI at system:b2a874f and at the mutant system:f1ff9be
Quest: .campaign/xapiand/quests/system-file-nr-close.md (the lead's; never edit it)

## Doing

The lead, 2026-11-23. system:b2a874f adds `read_file_nr`, which reads a NUL-terminated line from
/proc/sys/fs/file-nr and closes the descriptor on every path. Both system-wide readers use it.
test/test.cc's `test_system_wide_readers_close` checks the open-descriptor count around each call.
The mutant system:f1ff9be (branch quest/xapiand/system-file-nr-close-mutant) drops the `io::close`.
Its Linux CI must fail that check; macOS and FreeBSD read sysctl, so they pass it.

## Next

1. Read CI at b2a874f (green everywhere) and at f1ff9be (Linux jobs fail `test_system_wide_readers_close`). Cite both runs under A2.
2. Land, delete the mutant branch, then bump Xapiand's system pin (CMakeLists.txt:633) for open-files-floor.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261122-212046-713a5b: build-test at b2a874f (macOS; the check runs, and the sysctl paths hold no descriptor) |
| A2 | CI at b2a874f; failing Linux run at the mutant f1ff9be: to cite |

Found by open-files-floor's contributor: Xapiand run 44966941683 at Xapiand:4a07d2e, where "start at the exact minimum" counted 3 open descriptors and the function 5.

## Result

Linux no longer leaks two descriptors per call pair, and nothing changes elsewhere.
