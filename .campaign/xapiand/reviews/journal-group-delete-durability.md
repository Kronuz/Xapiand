# journal-group-delete review: durability

Commit: journal:d8453f6b67da28c01266485f0d046ce917d34736
Verdict: APPROVE
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A2 crash at every cut, four draws each, of the delete and of the open that finishes it:
  261123-010048-467c0f, GCC 261123-010040-e5a982, with the artifacts-first mutant failing
  (261122-215050-32f31b). It covers risk 1 for the delete itself.
- A1 reopen_empty and stays_deleted: 261123-010046-03aa5c, with the no-floor (261123-010057-6b2d97)
  and create-from-one (261123-010100-86e760) mutants failing. These cover the floor with no crash,
  across a reopen, a slot commit and a loop re-partition.
- A3 refused: 261123-010051-5f60a2. A7 loop_thread with the owner check on: 261123-010054-597440.
- GCC ASan+UBSan on every case: 261123-010043-3e6bf7. CI (TSan included) at d8453f6, per the brief.

## What I checked myself

Scripts are in `.scratch/xapiand/review-journal-group-delete-durability/`. `run.sh build`, then
`run.sh run P1..P5`, inserts `probes.inc` into host_delete_test.cc at the reviewed commit.

- Code read: `delete_group`, `create_group`/`load`, `update`, `commit`, `known`, `mark_lost`,
  `retired_floor`, `publish`, `repartition` (include/journal/host.h) and `create_shared` and
  `recover_shared` (include/journal/journal.h:569-627). Before this change, a nonzero `base` held
  exactly when the manifest was non-empty: `base` is set only in close_into_table (host.h:681),
  and reopen_group keeps the manifest. So R1's change to `known()` (host.h:829) alters nothing for
  existing groups. The turn driver already refuses a sequence at or below `retired`
  (turn_driver.h:269), which backs up the floor at staging time.
- Risk 1 and 2, the recreate under crashes (P3, job 261123-033404-75986a). The host is reopened on
  a deleted disk, so the floor comes from the shard file. The recreate and two appends are cut at
  every mutation, with four draws each. After every reopen, a is either none or holds only its own
  records, never an old one. A later append lands past the floor and reads back alone after a
  second reopen. 250 checks, 0 failures.
- Risk 2, segment retirement (P4, job 261123-035147-8d82b0). After a delete, a neighbor's traffic
  retired the log's old segments: only turns-1d and turns-1e remain. The deleted entry still holds
  floor 4, the recreate starts past it, and a recovers `{"", {fresh-1}}` alone. 4 checks, 0 failures.
- Risk 2, a lost or torn floor (read). The deleted entry commits through the shard's `.pending`
  file, sync, rename and directory sync. A torn `.pending` leaves the closed entry, and a damaged
  shard makes the id lost, which create_group refuses. `retired_floor` gives max for a damaged
  shard, and create_group refuses before `retired` is read, so there is no overflow. No path
  resurrects the group or reuses a sequence.
- Risk 3, space (P1, P2, P5, jobs 261123-033404-75986a and 261123-035147-8d82b0). This is not
  F1's shape:
  1. A delete on a full disk is refused with ENOSPC and the group stays closed, with every
     artifact. The delete succeeds once space is freed (P2).
  2. A host with no delete at all needs 16384 free bytes to open (P5 control). P1 and P2's
     full-disk open failures come from that existing requirement, not from the delete.
  3. With an unfinished recorded delete, the open needs 16384 to 16687 bytes, at most 303 bytes
     more than the control (P5). When the artifacts are still on disk, the open removes them
     before its commit.
  4. Artifacts are reclaimed by the delete or the finishing open. The old turn-log records are
     reclaimed by retention (P4).
- Risk 4, concurrency (read). delete_group, hold/settle, the slot's shard and its range table all
  run on the group's loop; `holding` is under `holds_mutex_`. A7 and CI's TSan cover the named
  interleaving. I found nothing that would justify a TSan probe.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | The doc says "a view of the deleted group lists nothing once the first commit publishes its range". The first commit publishes the deleted entry with its old incarnation and `[mark, high)`, so views list the old serials until the second commit. If that commit fails, views keep listing serials whose files are gone. | host.h:564-573, host.h:797-801 | Fix the doc's wording, or publish an empty range for a deleted entry. |
| N2 | no | nit | R4 and the doc say a failed first commit "leaves the group closed". A failure after the rename (for example, the directory sync fails with EIO) leaves the entry closed in memory, but the disk may hold it deleted, and a crash then yields deleted. Either outcome is whole, so this is wording only. | host.h:709-718, docs/mechanisms/host-delete.md "The steps" | State that it is closed in memory and either state on disk. |
| N3 | no | nit | Each create of a new id now reads its shard file twice: once in `status()` and again in `load()`. This matters for the many-indexes workload. | host.h:522-523 | Reuse the entry that `status` read. |
| N4 | no | gap | Deleted entries are never dropped, at 45 bytes each in the shard. Ids that are never reused grow the table without bound. This is documented in Limits. | docs/mechanisms/host-delete.md "Limits" | A follow-up once the log can report that a floor's segments are gone. |
| N5 | no | gap | This predates the quest: any Host open needs 16 KiB free, so a host on a full disk cannot open whether or not a delete ran. | P5 control, job 261123-035147-8d82b0 | A hunt-list item, outside this quest. |

## Not run

- TSan locally.
- Fault injection with EIO, as opposed to power cuts, on the delete's syncs and renames.
- A shard split or merge triggered by the delete's own commit; the crash base holds one shard.
- Platforms other than macOS, which are CI's.
