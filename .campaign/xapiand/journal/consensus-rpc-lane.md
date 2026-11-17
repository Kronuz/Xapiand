# consensus-rpc-lane: journal

State: design fixed
Quest: .campaign/xapiand/quests/consensus-rpc-lane.md (the lead's; never edit it)

## Doing

Design fixed: revision 2 at consensus:0fd8a7f (pushed), answering the design review at
Xapiand:e9efa08f3 (CHANGES): F1-F5, F8 blocking, F6, F7, F9-F11 folded; the map is the doc's last
table. No consensus sources touched.

## Next

1. Read the model jobs (Evidence); the lead re-reviews revision 2 (lens `design`).
2. The lead forwards to Detent's lead: Drain (kind 6) is new; T2 changed (three thread-safe pools
   with `ByteBudget::renew`, lane frames outside Bulk's connection window, FIFO small frames);
   T4 is the transport's own A5; T6 reworded; T8 (graceful close through Drain) is new.
3. After the transport's A7 lands: steps 5-6 of the doc.

## Blocked on

The lead: Q1 (file set: CMakeLists.txt, CHANGELOG.md, docs/design/rpc-lane*), Q2 (the `fuzz`
check needs CONSENSUS_FUZZ, LLVM clang and --no-tests=error; pattern rpc_stream_fuzz), Q3 (A4's
real remote search belongs to shard-rpc; here the three method shapes). Building waits on
detent:consensus-transport A7.

## Evidence

| Criterion | Evidence |
|---|---|
| A1-A7 | none yet (design only); each has its check in the design's criteria table |
| model, rev 2 | jobs 261116-210034-206f06 (drain,reorder; 1,553,861 states locally), 210037-5fa288 (early-end; 2,647,540), 210040-c06b73 (early-end,drain; 19,102,833, 5.7 GB, 638 s locally) |
| mutants, rev 2 (must fail) | 210043-6fe73c forget-is-violation, 210046-791b71 grant-without-reserve, 210049-4d4972 no-release-on-close, 210052-071016 no-regrant, 210056-be599d grant-on-receive, 210059-06d361 split-release, 210102-e23871 drain-ignored, 210106-216657 id-at-call |
| model, rev 1 | 261116-200021-283afb and mutants 200036-5f5bed..200051-577a89 (superseded) |

## Result

Revision 2: the one spec of the lane's bytes, each decision marked [transport] (consensus-transport
A7) or [rpc-lane]. Six kinds (Drain added), the full validity list (D3), atomic renewal of credit,
a sender pool, separate serving and calling pools, T1-T8. This quest changes no byte.

## Rulings I made

- R1 Lane on Bulk, not Control or a third connection (design D1).
- R2 "End" is a flag on Call/Body, not a sixth kind: one way to end a stream, one-frame calls.
- R3 Call ids by parity of the connection role (dialer odd), no direction bit.
- R4 Per-call windows only; no lane-wide connection window (the grantor owns the memory).
- R5 Overloaded calls are answered `Error overloaded`, never queued nor closing the connection.
- R6 The model lives beside the design as docs/design/rpc-lane-model.py (a design artifact, not
  consensus sources); the lead asked for one new file, the skill asks for an executable model.
- R7 (rev 2) Two receive pools, rpc_serving_bytes and rpc_calling_bytes, so served requests cannot
  take the bytes a host's own calls need (review F8's hold-and-wait).
- R8 (rev 2) Call.window must be nonzero; Drain's call field may be 0 (none accepted).
- R9 (rev 2) The model's rules run as three jobs; all three together outgrow a Python run.
