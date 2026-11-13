# RESTORE chore cap

Question (loop-scheduling, after fix (c)): with RESTORE's writes in chores and the light/heavy lanes, loaded light
queries run 2.69x slower than master at p99 (job 261112-225448-4dfa5e: 60.7-86.9 ms against 21.7-33.5 ms) while
RESTORE alone runs 8,471 docs/s against master's 4,603 (job 261112-225454-7d59c5; pre-7c recorded 6,076). Suspect:
RESTORE's chores (`children = min(shards, workers/2)`, prepare `width = workers - children`, `workers` = the work
pool's threads - 1) take every work-pool worker and the HTTP loops lose their CPUs.

Knob (this branch only): `XAPIAND_RESTORE_CHORES=N` caps `workers` at N, so children + width <= N
(src/database/handler.cc, `DocIndexer::index`). Uncapped on this host: 16 pool threads, 5 shards: 5 children +
10 prepares.

Host: macOS, 16 logical CPUs. Harness: `.scratch/rc/` (rc.sh, sampler.py, report.py), from loop-scheduling's
`.scratch/ab/`; binaries `rel-4738337a3` (master) and `rel-eef7a92e9` (this branch).

## Status

Waiting on jobs (all exclusive, at most 10 minutes, at Xapiand:eef7a92e9):

| Job | What |
|---|---|
| 261113-001647-f26c17 | release build of this branch (done, rel-eef7a92e9) |
| 261113-001854-f59ba1 | census: runnable threads (ps -M, 2 Hz) and per-thread-name busy fraction (sample) in T4/T5, master vs cap 15 vs cap 4 |
| 261113-001857-72429c | RESTORE docs/s (T12 loadtest), master and caps 15/8/6/4/2, mirrored order |
| 261113-002535-f5df4d | T4/T5 loaded light p99 and heavy searches, caps 15/8/4, mirrored |
| 261113-002537-7e5eae | T4/T5 loaded light p99 and heavy searches, caps 15/6/2, mirrored |

Next: `python3 .scratch/rc/report.py JOB_OUT` per job; confirm the cause from the census before reading the sweep.
