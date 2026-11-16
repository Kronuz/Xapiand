# Platform rows

Phase 7c's functional rows (`harness/gate7c_rows.sh`) run in CI on two platforms
besides Linux x86_64: arm64 Linux (`.github/workflows/arm64.yml`, GitHub's
`ubuntu-24.04-arm` runners, gcc-13) and FreeBSD 14.3 amd64
(`.github/workflows/freebsd.yml`, a VM on the Linux runner, clang). Each job
builds Release with `BUILD_TESTS=ON`, runs the whole CTest suite, builds an
ASan+UBSan tree, and runs the rows on both binaries. Each row's result is a
line of `rows.tsv`; the arm64 job uploads it with the row logs and a thread
dump (`harness/thread_stacks.sh`), the FreeBSD job prints each failed row's log.

## Status

| Platform | Run | Rows |
|---|---|---|
| arm64 | [43847919086](https://github.com/Kronuz/Xapiand/actions/runs/43847919086) at Xapiand:c85a1f6fd | 13 of 14 pass; `t1_census` waits for reactor:2ef28dc (reactor threads name themselves) |
| FreeBSD | [43847919063](https://github.com/Kronuz/Xapiand/actions/runs/43847919063) at Xapiand:c85a1f6fd | 11 of 14 pass; `t2_e2e_asan` and `t10_lua_asan` wait for system:563880a; `t10_lua` is open |

Findings:

1. `t1_census` (arm64): the 4 reactor threads never named themselves and
   showed as extra `Xapiand:MAIN` threads; fixed in reactor:2ef28dc.
2. `t2_e2e_asan`, `t10_lua_asan` (FreeBSD): `get_total_virtual_memory` read the
   32-bit `vm.stats.vm.v_page_count` into an uninitialized `int64_t`; UBSan
   aborted the node at its first `GET /:metrics`. Fixed in system:563880a.
3. `t10_lua` (FreeBSD Release): two of the four concurrent library-call loops
   answer 400 `Script exceeded its memory limit of 67108864 bytes`; the same
   row passes on arm64 Linux. Open.

The harness fixes the runs found: a free port for the foreign-script node, a
newman request timeout (`E2E_TIMEOUT_REQUEST`) for the slow ASan restore, a
`procstat -t` census on FreeBSD, `strata_check` built in both trees, and the
rows running as `nobody` on a writable scratch directory.
