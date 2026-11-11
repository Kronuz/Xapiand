# phase7b: Phase 7b, shards become groups

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase7b`.

## Outcome

Xapiand is distributed again, on strata's groups: each shard is a replication group, the journal
replaces Xapiand's WAL, the group replaces the replication protocol and `ELECT_PRIMARY`, placement moves
to the consensus authority, the shard RPC replaces the remote protocol, and the HTTP API exposes `ack`,
`read` and `refresh`. The distribution gate passes: the cluster and power-loss checks, the harness
beating current Xapiand on every row and Elasticsearch and Quickwit on the claimed rows at 3, 30 and at
least 300 nodes, and Track D's scale gates. Users' single-node data from the release moves onto groups
without loss. The lead splits it into quests when the owner resumes distribution.

## Acceptance criteria

From strata `docs/PLAN.md`, "Phase 7: Xapiand integration" (7b and its "Distribution gate"),
Xapiand `docs/design/xapiand-on-strata.md` section 5 (T11 for 7b), and gate 7a's carried items.

- A1 Shards become groups: the journal replaces the WAL, the group replaces the replication protocol and `ELECT_PRIMARY`, placement moves to the consensus authority, and the shard RPC replaces the remote protocol: tests in jobs
- A2 The HTTP API exposes `ack`, `read` and `refresh`, documented: the docs E2E in jobs
- A3 Gate: `harness/cluster_check.sh` passes, and a power-loss replacement for `harness/wal_recovery_check.sh` passes: jobs
- A4 Gate: the harness beats current Xapiand on every row, and Elasticsearch and Quickwit on the claimed rows (ingest, search and aggregation at 3, 30 and at least 300 nodes): jobs on a cluster the owner provides
- A5 Gate: Track D's scale gates pass (`track-d`): its landing
- A6 T11 for 7b: the group journal costs at most half of Xapiand's WAL per document, CPU and critical path; parity fails (owner, 2026-10-27, D6): `profile_bulk.sh` jobs
- A7 A single-node index from the release opens on a group without loss, and the upgrade path is documented: a migration check in jobs
- A8 Gate 7a's carried every-gate items (Xapiand `docs/gates/phase7a.md`, "Not run here"): Linux, FreeBSD and macOS arm64 sanitizer and TSan runs; the reasons for uncovered lines and tests for `strata/document.cc` and `strata/pattern.cc` up to 98/95; the phase's test-parity rows and the scale statement; a regression corpus replayed for the four fuzz targets; CR6 (search holds every shard lock): jobs
- A9 [artifact] The gate report, every row with its commit and jobs: `docs/gates/phase7b.md`

## Contracts

Consumes Phase 3R's `strata::group` (`phase3r`), Track C's consensus and journal (`track-c`), and Track
D's placement authority and query path (`track-d`); Detent's journal and consensus coroutine drivers
with Xapiand's four binding requirements (D21, Detent:a2d999a).

## Decisions

- Owner, 11-10 (D54): the durability design answers the remote-leader hop before a 2xx and what replaces `?commit` (journal ack plus refresh, and a checkpoint level) before the WAL and the debounced fsync are removed.

- Phase 7's gate splits (owner, 2026-10-29, D9): single-node 7a and 7c, then Phase 8 and the release;
  7b after the release. Blocked until then.
- Xapiand's WAL stays until 7b (7c Q2, D13); X1 removed the cluster, so cluster mode has no replication
  from X1 until 7b (the changelog says so).
- Parked inputs (strata `docs/status/README.md`, 2026-10-17, parked 2026-10-20, D4): Xapiand failover
  (`fix/failover`, SYS-14: promote the best live copy, restore the database-UUID check, a security
  review of the vote path) and the PKI helper (`design/pki`, SYS-2 defers peer identity to Track C's
  peer transport); this quest absorbs or retires them when it starts.
- Input (owner, 2026-11-03): the catalog's design has to be large-scale-proof for many nodes; the
  cold tier's catalog (`the strata lane`) and the name index (`name-index`) are single-node today.
- Risk critical: users' data moves from the release's WAL to a journal, and the wire protocol changes.

## May land early

Nothing that changes the release's on-disk data or wire.
