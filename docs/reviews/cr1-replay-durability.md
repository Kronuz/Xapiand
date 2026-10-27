# Xapiand review: CR1 WAL replay commit, durability check (scoped)

- Date: 2026-10-27.
- Reviewed: branch `fix/cr1-replay` at Xapiand:cff88d62c (Xapiand:43b7f8cf5, Xapiand:2ac8fda61,
  Xapiand:cff88d62c on Xapiand:88d37dbf0, base master Xapiand:829a67308); the diff
  `829a67308...fix/cr1-replay -- src/ harness/ tests/`; `docs/dev/cr1-replay.md`,
  `docs/mechanisms/wal-replay.md`, `docs/status/cr1-fix.md`.
- Checked against: AGENTS.md "Load-bearing invariants" (WAL replay idempotent w.r.t. revisions;
  `DB_UPDATED` triggers pull replication); the five questions of the lead's brief.
- Checked by: reading `src/database/shard.cc`, `src/database/wal.cc`, `src/database/pool.cc`,
  `src/engine/xapian/shard.cc`, `src/server/replication_protocol_views.cc`, `src/manager.cc`; a
  crash-point mutant, Release, Apple clang, `LTO=OFF`; 12 kill cases, all converged.
  - Probes, in the detached worktree `~/code/Kronuz/Xapiand-wt/review-cr1` (Xapiand:cff88d62c plus
    the mutant): `.scratch/cr1-crash-points.patch` (SIGKILL at a named point of the replay commit:
    `CR1_CRASH=replayed|storage|engine|walq|done`, `CR1_CRASH_N=k` for the k-th replay),
    `.scratch/crash_windows.sh` (the harness; `MODE=put` writes with PUTs instead of RESTORE),
    `.scratch/crash_windows.log` (RESTORE matrix, 8 cases; its trailing syntax error comes from editing the harness mid-run, after the last case), `.scratch/crash_windows_put2.log`
    (PUT matrix, 4 cases, lazily replayed index shards), `.scratch/crash_windows_put.log`
    (PUT, no kill, and a calibration), `.scratch/wal_hole_done4.txt` (the WAL left by the `done`
    kill), per-case logs and data in `.scratch/crash_windows_out*/`.
  - Not run: the cluster check, E2E and any sanitizer build (the developer's ASan results are
    taken as reported); any power-loss or fsync-level test (kill -9 keeps the page cache, so
    `DB_NO_SYNC` is invisible to it); replays of documents with stored blobs (the `storage` point
    ran with nothing to push); a two-node replica pull through the hole of D2; a mid-transaction
    reopen (D3, reasoned from code); benchmarks.
  - Not read (clean room): `src/server/remote_protocol_views.cc`, which holds the only other
    `begin_transaction` caller (D3).

## Verdict

**APPROVE WITH CONDITIONS: land after L1 and L2.** The replay commit converges from every crash
point probed and fixes CR1; the one behavior it makes worse is publishing a replay inside an open
transaction, a one-line guard.

What holds, and reproduces:

1. Crash windows converge (D1). SIGKILL before the commit, after `storage_commit`, after the glass
   commit, after the COMMIT line is queued and after the commit returns, on the nodes shard, each
   registry shard and the lazily opened index shards: the next start serves 200 documents,
   avg 19.5, max 39; a second crash replays nothing and changes nothing; a later write is replayed
   once on top (201, `fresh`=3); no WAL error, no revision mismatch, nothing quarantined.
2. The revision invariant holds: the replay runs lines of revision R, the commit moves glass to
   R+1 and writes COMMIT R (`shard.cc:926-1009`); a restart starts at R+1 (`wal.cc:166-282`).
3. The WAL is not truncated or rotated past the commit (D4); a replica pulling mid-reopen waits on
   the writable checkout and is fed only whole revisions (D5).
4. Node down on a failing commit of a startup shard is the right outcome (D6).

What blocks: L1 (D3), L2 (D2, D4, D7 doc corrections).

## Findings

### D1. Crash windows: converge (holds)

`.scratch/cr1-crash-points.patch` adds `cr1_probe(point)` at `shard.cc:495` (after
`execute()`, before the commit), `:980` (after `storage_commit`), `:988` (after the engine commit,
before the endpoint revision and the WAL line), `:1010` (COMMIT line queued, before `DB_UPDATED`)
and `:499` (commit returned). A start with RESTORE of 200 documents and kill -9 replays six shards
at startup (`.xapiand/nodes`, `.xapiand/indexes/.__1..5`); with PUTs the index shards
`replay/.__1..5` replay later, on the first read.

| Case (point #k, shard) | Next start | Second crash | Later write | Registry WAL tails |
|---|---|---|---|---|
| replayed #1 nodes | 200/19.5/39, 6 replays | 0 replays, same | 201, replayed once | all COMMIT |
| storage #2 .__1 | same, 5 replays | same | same | all COMMIT |
| engine #2 .__1 | same, 4 replays | same | same | .__1 ends REPLACE_DOCUMENT |
| walq #3 .__2 | same, 3 replays | same | same | .__2 ends REPLACE_DOCUMENT |
| done #4 .__3 | same, 2 replays | same | same | .__3 hole (D2) |
| engine #6, walq #6 .__5 | same, 0 replays | same | same | .__5 ends REPLACE_DOCUMENT |
| replayed #5 .__4 | same, 2 replays | same | same | all COMMIT |
| PUT: replayed #7, engine #7, walq #8, done #9 (index shards) | same, on the first read | same | same | all COMMIT |

Between replay and commit nothing durable changes, so the next start replays the same lines. After
the glass commit the glass revision decides and the replay is skipped, as the mechanism note says.
The developer's crash-replay-crash-replay only covered the points before the kill and after the
line was written; these cover the points in between.

### D2. A kill after the glass commit loses the COMMIT line for good (pre-existing; the doc overstates)

The COMMIT line is queued to the asynchronous WAL writer after the glass commit
(`shard.cc:977-987`). A kill anywhere from the glass commit until the writer appends the line,
including after `commit()` returns (the `done` point), leaves revision R's operations without their
COMMIT line, while glass is at R+1. The next write appends R+1 after them.
`.scratch/wal_hole_done4.txt` (`.xapiand/indexes/.__3` after the `done #4` case and the later write):

```
0 SET_METADATA / 0 REPLACE_DOCUMENT x3      (no "0 COMMIT")
1 REPLACE_DOCUMENT / 1 COMMIT
```

Locally this is harmless (D1). For a replica at R it is not: the sender gathers lines until a COMMIT
(`replication_protocol_views.cc:619-636`), so it sends R's lines with R+1's, and the replica's
`execute_line` throws "WAL revision mismatch" at the first R+1 line (`wal.cc:529`). Whether the
replica then falls back to a full copy was not checked. The window is the same for every
`Shard::commit`, so this fix does not introduce it; before the fix the replay was committed later
by the next write's commit, with the same window. The mechanism note's properties 1 and 2 together
claim the replica always gets the line; it does not across this crash. The fix belongs to the WAL
(on a writable open, append the missing COMMIT line when glass is one past the WAL's last revision),
not to this branch: C1.

### D3. A replay inside a transaction is now published (blocking: L1)

`reopen_writable` begins the shard's transaction on the new handle before the replay
(`shard.cc:449-451`); the new `commit(true, true)` then runs the engine commit in transaction mode,
which is `commit_transaction` plus `begin_transaction` (`src/engine/xapian/shard.cc:243-262`). The
operations replayed into that transaction become committed and visible, and a later
`cancel_transaction` no longer discards them.

Before this change (and pre-seam), the last open kept the replay inside the reopened transaction,
so the transaction's own commit published it and its cancel discarded it: the correct semantics.
The WAL has no BEGIN or CANCEL line (`wal.cc:547-581`), so it cannot tell a transaction's operations
from committed work; the question "are replayed operations always committed work" has no answer in
the WAL itself, and the developer's answer 3.1 assumes it. A cancelled transaction's WAL lines are
already replayed by any later restart; that is pre-existing and not this branch's to fix, but this
branch should not add a second path that publishes partial transactions.

Reachability: a reopen with `_transaction` set happens when a write or commit retries through
`reopen()` (`shard.cc:1000-1004`; `do_close(commit_)` keeps the transaction, `:868-873`). The
replication switch shard runs its transaction with `wal_=false` and an empty WAL
(`replication_protocol_views.cc:985-995`), so the only other WAL-active transaction is in
`remote_protocol_views.cc`, which I may not read. A restart is never transactional, so CR1 does not
need the transactional case.

Fix: commit only outside a transaction, `if (replayed && !is_transactional())`, keeping
`_modified` set either way, and rewrite property 3 of `docs/mechanisms/wal-replay.md` to say a
replay inside a transaction stays in it. The `engine_replay` cases stay as they are (they pin the
engine, not the policy).

### D4. Durability of the replay commit: the WAL stays the record, and is itself only async-synced

Nothing truncates or rotates WAL volumes by revision. The only deleters are a RESTORE shard's
checkin (`pool.cc:405-411`), the replica's whole-database switch
(`replication_protocol_views.cc:818`) and the replica reset in `replication_protocol_service.cc:164`.
So after the replay commit the WAL still holds every replayed line, and the next start would replay
them again if glass rolled back to R.

Two qualifications the mechanism note omits. First, the WAL is written in `STORAGE_ASYNC_SYNC`
mode (`wal.cc:69`, the debounced fsyncher) unless the shard is flagged synchronous, so after power
loss the WAL is durable only to its last fsync; property 5 calls it "the durable record" without
that. Second, a RESTORE checkin deletes the WAL with glass never fsynced (`DB_NO_SYNC`,
`src/engine/xapian/shard.cc:525`); if a RESTORE opened a shard that replays, the replay commit
shares that hazard. Both are pre-existing and apply to every commit; neither was tested (kill -9
only). Fix the note (L2); track the RESTORE-delete hazard (C2).

### D5. Replication ordering: safe

A replica's pull opens the primary's shard writable (`replication_protocol_views.cc:515`), so a pull
arriving mid-reopen waits for the reopening checkout to return. It then reads the glass revision
(R+1) and streams lines below it, sending a revision only when its COMMIT line is present
(`:619-636`). If the COMMIT line is still queued, nothing of R is sent and the replica stays at R; the
WAL writer sends its own `DB_UPDATED` after appending the line (`wal.cc:724-731`), so the next pull
gets it. The `DB_UPDATED` from `Shard::commit` is the earlier of the two and only starts a pull. The
remaining gap is the crash hole of D2.

### D6. Failure: node down is right for startup shards; the retry costs eleven replays

A failing replay commit takes the node down only for the shards opened at startup:
`.xapiand/nodes` (`manager.cc:1648`) and the registry. Serving instead would serve the registry
without its replayed schema, which is CR1's wrong answer, and a primary that cannot commit cannot
take writes anyway; going down lets another node take over. Fail-stop is the right call there.
Index shards replay lazily (the PUT cases replay on the first read), so their failure surfaces on
that request, not as a node down; readers that do not open the writer still see the last committed
state (`reopen_readable` never replays, `shard.cc:491-617`). The ledger's "the node does not come
up" holds for the startup shards only.

Two costs, not blockers. `Shard::reopen` retries `reopen_writable` DB_RETRIES+1 = 11 times, and each
attempt re-runs the whole replay before the commit fails again. And `Shard::commit`'s retry path
calls `reopen()` (`shard.cc:1000-1004`), which now calls `commit()` again from `reopen_writable`: on
a persistent `errc::closed` from the engine commit, each level opens a fresh 11-try loop and recurses
without a bound. A local glass commit does not raise `closed` in practice; C3 records it.

### D7. The quorum claim (doc only)

Property 4 says "without quorum `Shard::commit` throws `NotAvailable`, so a writable open waits for
quorum like any write". The replayed operations already check quorum
(`replace_document`, `shard.cc:1406`, and every other write), so the gate predates this branch, and
the open does not wait: `Shard::reopen` retries 11 times with no delay and throws. Fix the sentence
(L2).

### D8. Side finding: two-node numeric aggregations wrong with no kill (for the lead)

Probably not this root cause. No replay runs without a kill, and the ledger records the same
numbers with and without one. The same class (a handle or node that does not see the registry's
schema) is possible. One hint for whoever takes it: 494 + 297 = 791, not 780, so the two nodes'
shard sets overlap rather than partition the 40 documents, which points at the remote aggregation
merge or shard routing rather than at visibility. Not chased.

## Blocking items / Required changes

- L1 (D3): in `reopen_writable`, commit the replay only when `!is_transactional()`; rewrite
  property 3 of `docs/mechanisms/wal-replay.md` and answer 3.1 of `docs/status/cr1-fix.md` to
  match. Rerun `wal_replay_check.sh` and `wal_replay_agg_check.sh` after the change.
- L2 (D2, D4, D7): in `docs/mechanisms/wal-replay.md`, state that a kill after the glass commit can
  lose the COMMIT line for good, with the replica consequence (properties 1 and 2); that the WAL is
  async-synced unless the shard is synchronous (property 5); and correct the quorum sentence
  (property 4). Correct "the node does not come up" in the ledger to the startup shards.

## Conditions

- C1 (D2): record the lost-COMMIT-line hole as a tracked WAL issue (pre-existing, every commit), with
  `.scratch/wal_hole_done4.txt` as its repro; the fix is a WAL repair on writable open.
- C2 (D4): record the RESTORE-checkin WAL delete over an unsynced glass as a tracked durability
  issue (pre-existing).
- C3 (D6): record the unbounded `commit -> reopen -> reopen_writable -> commit` recursion on a
  persistent `closed` error; bound it when the retry loops are next touched.
- Gate 0.5's rerun may proceed on the branch once L1 and L2 land; none of C1 to C3 gates it.

Not run: cluster check, E2E and sanitizers on this commit; power-loss tests; blob-storage replays; a
two-node pull through D2's hole; a mid-transaction reopen; benchmarks.
