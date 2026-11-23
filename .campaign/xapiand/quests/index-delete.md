# index-delete: Index DELETE closes and deletes the index's group

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status index-delete`.

## Outcome

`DELETE /<index>/` removes the index. Today `delete_database_view` answers 501. After this quest,
DELETE closes the index's shard groups, deletes each group's journal state with
`Host::delete_group`, and removes the index's directory, then answers success. A later PUT at the
same path creates the index anew, with an empty group: no record of the deleted index's commands
replays into it, after a clean restart or a crash.

## Acceptance criteria

- A1 DELETE then recreate at the same path starts empty: no documents, no schema, no journal
  records from before, both after a clean restart and after a kill -9 at each step of the delete
  (a crash sweep): a test in tests/test_pool.cc or tests/test_index_delete*, run by the suite
- A2 The same test fails without `Host::delete_group` (a `--fails-without` job)
- A3 DELETE of a missing index answers 404; DELETE of a nested index's parent (`twitter` with
  `twitter/user` present) deletes only the parent's own shards, leaving the child readable
- A4 [artifact] The API reference documents DELETE with its responses, and a docs-as-E2E example
  covers delete then recreate

## Contracts

Consumes strata's group lifecycle (open, close) and journal's `Host::delete_group` through
strata-journal-pin. Adds a user-visible verb that answered 501.

## Decisions

- Lead, 2026-11-23: its own quest (journal-replaces-wal's ruling, option ii). That quest's outcome
  is the WAL's removal; DELETE is a feature, and 501 is today's behavior on master.
- Lead, 2026-11-23: nested indexes are core: deleting a parent never deletes a child index.

## May land early

Nothing: one user-visible verb, reviewed once.

## Tasks

None: one contributor (implementation, strong, high).
