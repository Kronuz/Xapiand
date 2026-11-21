# turn-log-parity--group-commit: journal

State: working
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

Reproducing the gap at the quest head (strata:330bfd41): `test_barrier_cohort` extended to
configurations of K writers on I indexes on L loops; the shared-index case (K writers on fewer
groups) is expected to fail there.

## Next

1. Per-writer cohort in `turn_sealer` (released waiters, arrivals by released writers' joins).
2. Jobs: the test at the head (fails-without), at the fix, suite; CI TSan.
3. Smoke probe-b K=2,4,8 on 1 log and K logs before/after (light, not the deciding numbers).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A4 | <pending> |

## Result

<pending>

## Rulings I made

- A4 vs D7 (rocksdb-parity task 5): A4 does not subsume D7. A4 bounds the barriers a waiting
  writer rides (the turn's timing against arrivals); D7 bounds the bytes a log admits and packs
  several records of a group into one turn (`turn_records`, the `ack=none` window). They share
  `log_state::join` and `seal`, but neither test proves the other: task 5 stays its own task.
