# cluster-migration: One node to a cluster and back, without loss

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster-migration`.

## Outcome

A single node with data seeds a cluster (design D13, phase7b A7): its indexes are lone-voter groups,
and raising `number_of_replicas` grows them by install onto new nodes. A shrink, a drain and a
restart past log truncation lose nothing. Pre-release data directories holding a WAL are not
converted (D87); the upgrade page says so and gives the dump-and-restore path.

## Acceptance criteria

- A1 A node with indexes, stored blobs included, seeds three nodes; `number_of_replicas` 0 to 2;
  digests equal across replicas, stored blob bytes included, and no document lost (a job)
- A2 A shrink back to one replica and a drain of a seeded node lose nothing (a job)
- A3 A node restarted after its group's log was truncated past its position catches up by install,
  and its digest equals the leader's (a job)
- A4 Under the load that broke `detent:consensus-fetch-macos`, A1 passes on macOS processes
  (a job)
- A5 [artifact] `docs/src/content/docs/upgrade.md`: one node to a cluster, a cluster back to one
  node, and pre-release data (reindex or restore)

## Contracts

Consumes `strata:placement-repair` and `xapiand:shard-map`. Provides nothing new on disk or wire.

## Decisions

- Design D13, F10 (`upgrade.md` is this quest's alone).

## May land early

None.
