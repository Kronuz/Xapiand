# group-catalog: a replication group runs on its catalog slot's index

Risk: high (durability, concurrency). The lead writes this quest; contributors read it and never edit
it. Its machine state is in `state.json`: `campaign status group-catalog`.

## Outcome

`strata::group` opens and owns its own index today (group.hh:835), while Xapiand's catalog owns
each index by value (catalog.hh:347), and one directory cannot be opened twice. With D169, a group
borrows its catalog slot's index: `group::config` takes the index, recover uses it, and the group's
close leaves it to the catalog. The catalog's commit chore for a group's index runs the group's
checkpoint, and its close runs the group's close before the index's. The group's chore runs on a
lane apart from the catalog's transitions, since a sync wait from the commit chore on the group's
chore would otherwise deadlock. journal-replaces-wal's core (every shard a lone-voter group) waits
on this.

## Acceptance criteria

Gate: the suite (`build-test`) and CI (`asan`, `tsan`).

- A1 A group opened on a catalog slot's index recovers from it, applies, and its close leaves the
  index open and usable by the catalog: a test with values
- A2 A catalog close of an idle group publishes a checkpoint bundle covering everything applied
  (group-core A6's clause): a test with values; a mutant whose close hook skips the group's
  checkpoint fails it (`--fails-without`)
- A3 A commit chore that waits on the group's checkpoint completes, because the group's chore runs
  on its own lane: a test with a deadline that fails with both on one lane (`--fails-without` on
  that mutant); TSan in CI
- A4 [artifact] `docs/mechanisms/group-catalog.md` explains the borrow, the hooks and the lanes

## Contracts

`strata::group::config` (it takes a borrowed index) and the catalog's hooks for a group's index;
Xapiand's journal-replaces-wal consumes both.

## Decisions

- D169 (lead, 2026-11-21).

## May land early

None.
