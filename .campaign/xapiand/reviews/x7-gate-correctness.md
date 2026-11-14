# x7-gate review: correctness

Commit: Xapiand:4909eae7e
Verdict: CHANGES
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The report is honest where it scores: every number I traced matches its job log, and every MISS and
"not shown" carries its real value. What blocks is where it scores: master has moved under it (the
strata pin), the timing jobs do not build what they time, and two carried rows sit on the engine
before the strata bump while the status line says otherwise.

## Evidence I relied on

- Rows Xapiand:0a0321691: 261113-111808-93efd0 (builds both binaries itself; 14 of 14 rows, ctest
  ASan+UBSan 39 of 39), corpora 261113-111821-2f9e75 (`--after` the rows job, same slot and commit),
  checks 3cf199, 3013ac, 1a49cf, f9dc7f, e12ef2. All build in-job at a commit whose `src`, CMake and
  tests equal master Xapiand:6ede7a0ce's (`git diff 6ede7a0ce 0a0321691 -- . ':!docs' ':!harness'
  ':!.campaign'` is empty; same for 0a0321691..6dcc770ff and 6dcc770ff..4909eae7e).
- Linux 261113-130015-3cce93 (git archive of 6dcc770ff, built on the VM in-job).
- Timing 90f8a3, 4f5407, 90ebdc, 736919, 4ec27a: values match their logs (below); the binary they
  time is a prebuilt one (finding F2).

## What I checked myself

Read from job logs under `~/.scratch/campaign/jobs/<id>/a1/` (no rerun needed):

- T4 (90f8a3): every head arm has all 16 HTTP loops over 1,024 us (arm 2: 16 of 17 loops, arm 3:
  17 of 17), worst p99 6,144 us (http2); pre-7c only `control` at 1,280 us. Matches.
- T5 (90f8a3): client unloaded p99 3.90 / 4.24 ms (limit 2 x 4.07 + 1 = 9.14 ms), loaded 36.3 / 28.4
  ms, server-side 40.2 ms median, head/pre 0.070. Matches.
- T12 (4f5407, 90ebdc): RESTORE 1.121 (1.030-1.141), QPS 2.642; bulk 1.052, arm 2's rc 3 is
  `INVALID PROFILE: 147 wall samples`, its smoke stands. Matches; every pair above 1.0.
- T10 (736919): flood p99 45.61 ms, max 48.55 ms; T4 beside 4 scripts 6,144 us. Matches.
- A16 (4ec27a): p50 23,136 ms, p99 29,177 ms, 200 open, 107.6 KiB each, SR3 lines. Matches.
- T6, T1, C5 on Linux (3cce93): 292 and 76,249.74 per 1,000; census MAIN 9, W 8, FS 5, WL 4, LOG 1;
  `t10_lua` the only failing row. Matches.
- T8 (93efd0 rows): 6 rounds of 2,500 against 2,000, 500 refused each; registry 503s 4,160 light,
  11,676 heavy; 0 other. Matches.
- T2 (93efd0): 761 of 761 docs assertions, GREEN; no "blocks on database work" line in the node
  log, so no guard hit in this (asserts-off) build.
- Which binary each timing row timed (risk 1): every x7-timing job runs `$PWD/build-cur/bin/xapiand`
  and none of them builds it. Only two jobs built `build-cur`: 261111-171014-19268b at Xapiand:49ebb4481
  (src equal to master 27d45f101, strata 5d742b24) and 261113-111748-e52352 at Xapiand:0a0321691
  (src equal to 6ede7a0ce, strata 86fe7c47; it built `xapiand` and then exited 1 on build-pre's
  configure, so pre and pre-hop stay 19268b's). So every 11-11 timing number (c9ab68, 7a746f,
  06e893, c39d8d, a82ab4, 2567dc) is 27d45f101's source and every 11-13 one is 6ede7a0ce's, whatever
  commit the job names. The x7-rows, x7-x35, x7-shape and Linux jobs build in-job.
- D16 against `src/manager.cc` `join()` (A18): one deadline, Stops 1 to 5 in that order, the grace
  formula at :165, `_Exit(EX_TEMPFAIL)` at :1195. Matches, with two nits (N8).
- Probe 261113-162747-f54d7a (`.scratch/review-x7-gate-correctness/t2_asserts_e2e.sh`): T2's docs
  E2E on an asserts-on ASan+UBSan build, the design's procedure (section 4: "on a Debug ASan build").
  Result: exit 0 in 933 s, `Option ASSERTS - on`, 761 of 761 assertions GREEN, and no assert, guard
  ("blocks on database work"), ASan or UBSan line in the node log. T2 holds at the design's procedure.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The report is scored at master 6ede7a0ce's `src`; master now pins strata 397c02d5 (Xapiand:b6a25cf18) instead of 86fe7c47. That range is sync-at-first-commit (strata 34aa3691, cb5294b8: an open leaves its inherited log and directory sync to the first commit) and faultfs changes: a change in when data reaches the device, under T3 and T7's durability claims and every timing row that opens or commits. `campaign status x7-gate` already lists "merge master into it". | `git diff 6ede7a0ce master -- CMakeLists.txt tests/test_node.cc`; `git -C strata log 86fe7c47..397c02d5` | Merge master; rerun the rows job, `recovery`, `t7-tsan`, Linux 3cce93's command, and the timing jobs (after F2) at the merged head; rescore T3, T4, T5, T7, T10, T12 and A16 from them. |
| F2 | yes | defect | Timing jobs time a binary they did not build. 90f8a3, 4f5407, 90ebdc, 736919 and 4ec27a name Xapiand:6dcc770ff but run `build-cur` from e52352 at 0a0321691 (src-equal here, so their values stand for 6ede7a0ce's src). The same mechanism attributed A16's 108 ms to 77d649648. A landing-verify `campaign job rerun` of these jobs at the merged head would re-time the stale 6ede7a0ce binary and report it as the new code. | spec.json `cmd` of each job (no `cmake --build build-cur`); 19268b and e52352 are the only builders of `build-cur` since 11-11 | Each timing job builds `build-cur` itself (ccache keeps it cheap), or the build writes a stamp (`git rev-parse HEAD:src`, plus the CMakeLists blob) and the timing job exits non-zero when it differs from its checkout's. Test: the rerun's log prints the stamp equal to its own commit's. |
| F3 | yes | defect | A16's row is wrong now. It calls the 214x "unattributed" and lists `src` files as suspects; the bisect (explore/a16-bisect, `docs/perf/a16-many-index-regression.md`) attributes it to the strata bump 55741956f (27d45f101 98 ms, 5c0738825 108 ms, 55741956f 19,977 ms), and the fixed pin gives p50 884 ms, p99 1,011 ms at the default pool (261113-155318-cb2a87) and 132 ms at pool 5,000 (261113-155320-bf8f29). The "Xapiand:77d649648" comparison was a binary built at 49ebb4481 (src-equal to 27d45f101). | job logs cb2a87, bf8f29, bc89da; bisect note | See "What A16 needs" below. |
| F4 | yes | gap | The status line says "every row scored at master Xapiand:6ede7a0ce's `src`", but T13's per-open-index cost (x3-5 d979c7, at 03ba6478a: 27d45f101's src, strata 5d742b24) and T11 (7a746f, 27d45f101's src) predate the strata bump. The open path changed twice since (manifest-log open with a device flush, then sync-at-first-commit), and master's own A16 run shows 107.6 KiB per open index at 200 open against T13's 35.1 KiB. D89's pool-from-memory sizes the pool from this number. | d979c7 and 7a746f spec.json commits; `git diff --stat 03ba6478a 6ede7a0ce -- src CMakeLists.txt` (21 files); 4ec27a log | Rerun d979c7's command (it builds in-job) at the merged head and restate T13's per-open-index cost there; label T11 "at 27d45f101's src" or rerun it after F2. |
| F5 | no | nit | T2's rows build has asserts off, while the design asks for a Debug ASan build, and CI's E2E jobs stay asserts-off citing "a known pre-existing assertion on the restore write path". The probe shows that assertion no longer fires: the asserts-on ASan+UBSan E2E is GREEN. | 261113-162747-f54d7a; `.github/workflows/ci.yml:173-177`, `:235-238` | Build `t2_e2e_asan` with `-DASSERTS=ON`; drop CI's stale comment and turn asserts on there (a separate quest's call). |
| N1 | no | gap | `campaign status x7-gate` shows A4, A5, A10 and A16 "met" because their tagged jobs pass under `--no-assert`, while the report scores them MISS or not shown. Read the report, not the status, for the gate's verdict. | `campaign status x7-gate`; for.json of 90f8a3, 736919, 4ec27a | Tag the timing jobs to their criteria only when the job asserts the target, or note it in the journal. |
| N2 | no | nit | T5's MISS compares the server-side loaded p99 (40.2 ms) with a limit built from the client-side unloaded p99. The like-for-like client-side loaded p99 (32.3 ms) misses too, by 3.5x; lead with that. | 90f8a3 log | State the client-side ratio first. |
| N3 | no | nit | The Perf carried row still says "T12's RESTORE miss stands"; T12 now passes. | `docs/gates/phase7c.md`, last row | Drop the clause. |
| N4 | no | nit | T11's wall half is not measured on head: `profile_light.sh --compare` prints `-` for head's dispatch wall and scores it 0. The PASS on wall holds by construction (no hop), which the row should say. | 7a746f log ("dispatch wall us (median) 25.052 - 0.000") | Say "wall: head has no hop to time". |
| N5 | no | nit | On Linux `gate7c_rows.sh`'s census check enforces no count for `Xapiand:MAIN`, which every unnamed thread inherits; an extra pool would pass as MAIN. This run's 9 (8 reactors and main) is right. | `harness/gate7c_rows.sh:55-58` | Check MAIN equals reactors + 1 on Linux, as macOS checks `unnamed`. |
| N6 | no | nit | The Linux job builds Lua from `~/ls-loop-scheduling/lua-src-patched` on the VM (`FETCHCONTENT_SOURCE_DIR_LUA`), not master's pinned fetch, and the report does not say so. | 3cce93 spec.json | Use the pin (88139a40e added the mirror) or state the copy. |
| N7 | no | nit | T5's pre-7c arm 1 still shows generator wait p99 95.7 ms, so one pre-7c pair carries generator stall; the "below pre-7c" half holds by 14x either way. | 90f8a3 `gen_wait` line | None needed beyond a note. |
| N8 | no | nit | D16 says Stop 3 polls "in slices of at most 500 ms"; the code polls 1-10 ms (`src/manager.cc:1133`). Stops 4 and 5 wait without the deadline (`:1160`, `:1179`), so a hung log device makes the stop unbounded despite "one deadline". | `docs/design/phase7c.md:408`; `src/manager.cc` | Fix the text; bound Stops 4 and 5 or say they are outside the bound. |

## What A16 needs now

1. Its row restated with the cause (the strata bump at 55741956f, fixed by Xapiand:b6a25cf18) and
   the build behind each number (2567dc: 49ebb4481's binary).
2. One strict `wide_search_check.py gate` job at the merged head on a binary that job builds (F2),
   default pool, reported as is. Expect about 884 ms p50 and 1,011 ms p99 (cb2a87): 8x the 108 ms the
   old build got by piling 1,382 indexes open, so the row stays a reported value, not a pass. The pool
   5,000 number (132 ms) beside it.
3. The 10,000-index target is still not shown on master's code. To fit D59, create the 10,000 indexes
   in a light job and time the searches in a strict job `--after` it on the same data directory; or
   the lead records 2,500 as the stated scale.
4. SR3 stays "not closed" unless the 99 ms max is attributed: the max counts since node start, which
   includes the opening search; take it over the timed searches only.
5. The per-open-index cost from that run (or F4's rerun) replaces 107.6 KiB in the row.

## Not run

- No rerun at the merged head (F1); no timing job of my own.
- No check of T13's coverage job (261111-144409-ecb5b2) beyond reading the row; no check of A21's
  Linux and macOS values against their logs (both are fixed-commit A/Bs, unaffected by F2).
- No review of the security-lens items (SR1, SR2, SR4 to SR7).
- FreeBSD and arm64: not run by anyone (stated misses).
