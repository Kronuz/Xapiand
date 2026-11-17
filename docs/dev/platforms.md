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
| arm64 | [43983942361](https://github.com/Kronuz/Xapiand/actions/runs/43983942361) at Xapiand:26d4a14f6 | all rows pass |
| FreeBSD | [43983942535](https://github.com/Kronuz/Xapiand/actions/runs/43983942535) at Xapiand:26d4a14f6 | all rows pass |

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
