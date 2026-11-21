# Crash scenario D's wrong row (FreeBSD t3_crash_asan at Xapiand:a465288dc)

State: not reproduced so far; the repeat jobs below are queued.

## Target

FreeBSD CI run 44488881851 (master, Xapiand:a465288dc), row `t3_crash_asan`:
"D recovered 4501, 1 wrong, 0 duplicated, 0 old acknowledged writes missing, 0 gaps below a
writer's highest id". The strata check passed after both of D's kills, and `t3_crash` (Release)
passed in the same run. The harness printed nothing about the row.

What the counts already rule out. A wrong row is skipped before the writer's set is built, so a
writer document `wA-M` with a wrong or missing `n` leaves `M` out of writer A's set. With 0 gaps,
`M` would have to be above every other `n` recovered for A: the last document of A's sequence,
the torn tail a crash leaves. The other shape is a row whose id is not `w<writer>-<n>` at all,
and the only such documents the harness writes into `multi` are the 40 pokes (`pokeK`, deleted
before the listing). So the row is one of:

1. A poke whose DELETE did not take (`pokeK None`). `poke` ignored every response, so a DELETE
   answered with an error left no trace. This is a harness or request-handling issue, not
   durability.
2. Writer A's last recovered document with a wrong `n` (`wA-M None` or another `n`): a torn or
   mixed document out of WAL replay, a durability bug.

## Harness change (explore/crash-wrong-row)

- `harness/engine_crash_check.sh`: a wrong row in B and D prints its listing line, its hit, and
  the document's raw `GET` response (status and body bytes); in D also its writer's
  acknowledgement time against the kill. `list_set` keeps every search page's body byte for byte
  (`OUT.pages`). `poke` prints the HTTP status of any PUT, DELETE or commit that is not 2xx, which
  tells case 1 from case 2 on the next failure. `--only D` runs one scenario.
- `harness/engine_crash_repeat.sh KIND N`: a mutant first (the DELETEs shifted by one id, so
  `poke0` stays) must fail and print `'poke0 None'` with its `GET 200` bytes; then scenario D N
  times, keeping each failing run's rundir and log.
- `docs/perf/crash-wrong-row-vm.sh`: the same on the campaign's FreeBSD VM (port 18784), built as
  CI's `freebsd.yml` builds and run as `nobody` as CI's rows run.

## Evidence

GitHub CI, `freebsd.yml`, every run since a465288dc (187 runs that finished, through
2026-11-21 04:00Z): 153 more `t3_crash_asan` and 154 more `t3_crash` rows passed, so 307 scenario D
runs on FreeBSD 14.3 without a repeat. The one failure is still 44488881851's.

macOS, earlier repeats at Xapiand:e4884d358 (the whole check, A, B and D): 0 of 40 Release
(`261119-042149-e9b656`) and 0 of 40 ASan+UBSan (`261119-042150-86567e`).

| Job | Where | Build | Trials | Wrong rows |
|---|---|---|---|---|
| 261120-203705-507e50, -203706-ba9491, -203708-3af3a3 | macOS | Release | 3 x 100 | pending |
| 261120-203709-edb138, -203710-b99d9a, -203711-5afaec | macOS | ASan+UBSan | 3 x 70 | pending |
| 261120-203712-6c1690 | FreeBSD VM | build | | pending |
| 261120-203713-c2f507, -203716-6a46fb | FreeBSD VM | ASan+UBSan | 2 x 60 | pending |
| 261120-203715-5786c5, -203717-8fdb98 | FreeBSD VM | Release | 2 x 100 | pending |

## Verdict

Pending the jobs.
