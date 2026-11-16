# offload-followups: journal

State: landed
Quest: .campaign/xapiand/quests/offload-followups.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:201414347 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/offload-followups.md`: "`last_seen` advances during a held control-worker job (`Node::touch_sender` on the loop)" |
| A2 | `docs/status/offload-followups.md`: "a job queued behind a 1000-command flood runs after at most 64 commands" |
| A3 | `docs/status/offload-followups.md`: "CR5: reactor pinned to main `0c04d0d`" |

## Result

Landed at Xapiand:201414347 on 2026-10-26. `tests/test_offload_followups.cc`: 14 checks; a queued job behind 1000 commands runs after at most 64. No review named in the ledger.

- Merge: Merge branch 'fix/offload-followups'.
- Xapiand:f7486b7ff -- code and ledger; next commit makes the drain test independent of batch size.
- Mutants: missing touch fails 2 checks; `drain_batch` 1e9 fails 2; broken `drain_bounded` fails 1.
- ASan unit tests: blocking guard 14, pool 21, engine shard 52, engine document 844, endpoint 16; all OK.
- Cluster checks 2 and 3 GREEN.
- Open: on-message wiring is covered by code reading and cluster runs, not a revert check; code review and merge remain.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
