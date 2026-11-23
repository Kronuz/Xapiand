# group-catalog: a replication group runs on its catalog slot's index

Risk: high (durability, concurrency). The lead writes this quest; contributors read it and never edit
it. Its machine state is in `state.json`: `campaign status group-catalog`.

## Outcome

`strata::group` opens and owns its own index today (group.hh:835), while Xapiand's catalog owns
each index by value (catalog.hh:347), and one directory cannot be opened twice. With D169, a group
borrows its catalog slot's index: `group::config` takes the index, recover uses it, and the group's
close leaves it to the catalog. The catalog's commit of a group's index stays a strata commit (it
only raises the published head; recovery per D16 holds). A new catalog hook, `close_payload`, runs
the group's close (a checkpoint covering everything applied) outside the index's write lease, right
before `index.close()`. A commit hook must never sync-wait on the group's checkpoint: the catalog
runs commit hooks inside `chore_scope`, and the group's chore takes the write lease, which waits
while the chore bit is held (owner.hh), so it deadlocks on any lane (the contributor's msg-201). journal-replaces-wal's core (every shard a lone-voter group) waits
on this.

## Acceptance criteria

Gate: the suite (`build-test`) and CI (`asan`, `tsan`).

- A1 A group opened on a catalog slot's index recovers from it, applies, and its close leaves the
  index open and usable by the catalog: a test with values
- A2 A catalog close of an idle group publishes a checkpoint bundle covering everything applied
  (group-core A6's clause): a test with values; a mutant whose close hook skips the group's
  checkpoint fails it (`--fails-without`)
- A3 A catalog close of a group's index completes, with the group's close run by `close_payload`
  outside the write lease: a test with a deadline; a mutant that runs it from the commit hook inside
  `chore_scope` fails by its deadline instead of hanging (`--fails-without`); TSan in CI
- A4 [artifact] `docs/mechanisms/group-catalog.md` explains the borrow, the hooks and the lanes

## Contracts

`strata::group::config` (it takes a borrowed index) and the catalog's hooks for a group's index;
Xapiand's journal-replaces-wal consumes both.

## Decisions

- D169 (lead, 2026-11-21), revised the same day on msg-201: close, not commit, runs the group's checkpoint.

## May land early

None.
