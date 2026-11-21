# journal-group-delete: a host deletes a left group's journal, so the node can rejoin it

Risk: high (durability). The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status journal-group-delete`.

## Outcome

`journal::Host` cannot delete a group's journal, so a node that left a group can never rejoin it:
the old segments and manifest stay, and the group cannot reopen empty there (placement--repair,
11-21: a drain or replace moves a replica off a node and later back). Host deletes a closed
group's segments and manifest durably, and a later open of that group starts empty (Detent's
lead, msg-187, 11-21). consensus's half is Detent's consensus-replica-delete, after this quest: a
replica removed from membership deletes its log and image and rejoins as a fresh learner. The
contributor sends the delete API's final shape to Detent's lead once it is fixed.

## Acceptance criteria

- A1 A deleted group reopens empty and accepts writes, and the other groups on the host keep every
  acknowledged record: a test with values
- A2 A power cut at every step of a delete leaves either the whole old group or none, never part
  of one, and the other groups intact: the crash model (faultlab's faultfs) at every cut point,
  with a mutant that deletes segments before the manifest record, `--fails-without`
- A3 Deleting an open group, or one with an outstanding pin, is refused with a named error: a test
- A3b `delete_group` called on the group's own loop right after `close_group` succeeds with no
  handoff: a test
- A4 The suite, ASan+UBSan and TSan pass: the suite job and CI
- A5 [artifact] `docs/mechanisms/host-delete.md`, and CHANGELOG.md names the new call
- A6 [ci] CI is green on every platform

## Contracts

Provides journal:host (a new delete call; nothing existing changes). Consumed by placement
(repair's rejoin) and detent:consensus-replica-delete.

## Decisions

- Detent's lead, 2026-11-21 (msg-187): no Detent quest covers it; filed in journal on our side,
  after journal-idle-groups and journal-loop-handoff, which change Host and its range table, and
  after journal-tail-second-sector, which fixes a silent loss of acknowledged turns in src/turn_log.

- Lead, 2026-11-21: shape proposed to Detent's lead, to change only with a reason sent back:
  `journal::Host::delete_group(std::uint64_t group)`, synchronous like `create_group`; refused with a
  named error unless `status(group)` is closed and `holding(group)` is false; a table record marks the
  group deleted before its segments, manifest and artifacts go, and `open()` finishes a recorded
  delete; a later `create_group(group)` starts empty.
  Detent's lead, same day: callable from the loop thread that owned the group, right after
  `close_group` on that loop (consensus's Driver runs there), with no handoff; no notification.

## May land early

Nothing: one landing.
