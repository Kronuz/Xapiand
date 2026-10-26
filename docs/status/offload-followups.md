# offload-followups (offload-bridge CR2, CR5 leftovers)

Branch `fix/offload-followups` from Xapiand master `55316114d` (local only, never pushed). Follows
`docs/status/offload-bridge.md` and `docs/reviews/offload-bridge-code.md` CR2 and CR5.

## State

In progress.

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

## Next

## Blocked on

Nothing.
