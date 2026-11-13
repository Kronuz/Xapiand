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

Jobs queued: build 261113-001647-f26c17; census 261113-001854-f59ba1; RESTORE sweep 261113-001857-72429c.
