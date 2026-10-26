# offload-followups (offload-bridge CR2, CR5 leftovers)

Branch `fix/offload-followups` from Xapiand master `55316114d` (local only, never pushed). Follows
`docs/status/offload-bridge.md` and `docs/reviews/offload-bridge-code.md` CR2 and CR5.

## State

CR2a, CR2b and the CR5 leftovers done and verified (Debug ASan, Apple clang 17, macOS x86_64). Not run: Linux, FreeBSD, timing.

## Plan and design

- CR2a: `Discovery::on_message` refreshes the DB_UPDATED sender's liveness at message time through
  `Node::touch_sender` (`src/node.cc`), on the bus loop (one mutex-guarded map update, no database
  work, so it passes the blocking guard), and dispatches only when it returned a node and the state
  is READY. `Discovery::db_updated` on the worker no longer touches; it looks the sender up.
- CR2b: `dispatch_command_async_cb` runs `engine::drain_bounded` (`src/engine/drain.h`): at most
  `engine::drain_batch` (64) commands per control-worker job, then it re-posts itself behind
  whatever the worker already has queued, so a queued job waits for at most 64 commands.
- CR5: reactor pinned to main `0c04d0d`; the "lead repins" comment is gone.
- D13: the design lives in the strata repo (`strata/docs/design/xapiand-on-strata.md`, outside this
  branch); in this repo D13 appears only in reviews and ledgers. The amended text, with the remote
  residual, is in `docs/status/offload-bridge.md` ("D13 as amended (C1)") and stays the lead's to
  apply in strata.

## Done

- Code and ledger: `f7486b7ff`; drain test made independent of the batch size: next commit.
- Tests (`tests/test_offload_followups.cc`, 14 checks, ASan): `last_seen` advances during a held
  control-worker job (`Node::touch_sender` on the loop); a job queued behind a 1000-command flood
  runs after at most 64 commands; `drain_bounded` contract.
- Mutants (`.scratch/revert_checks.sh`, output in `.scratch/revert_checks.out`): `touch_sender`
  without the touch: 2 checks fail; `drain_batch` 1e9: 2 fail; `drain_bounded` never reporting more:
  1 fails. All sources restored.
- ASan unit tests, fresh `.scratch/build-asan`: `blocking_guard` 14, `pool` 21, `engine_shard` 52,
  `engine_document` 844, `endpoint` 16, all OK. The reactor `0c04d0d` pin builds.
- `harness/cluster_check.sh 2` and `3` on the ASan binary: GREEN (discovery, cross-node read,
  distributed search; an ASan report would kill a node).
- Limit: `Discovery::on_message` is not linkable in the unit tests, so the wiring (touch before
  dispatch) is covered by reading and the cluster runs, not by a revert check; the unit test covers
  `Node::touch_sender`.

## Next

Code review of this branch; the lead merges.

## Blocked on

Nothing.
