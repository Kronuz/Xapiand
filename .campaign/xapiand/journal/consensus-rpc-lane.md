# consensus-rpc-lane: journal

State: design ready for review
Quest: .campaign/xapiand/quests/consensus-rpc-lane.md (the lead's; never edit it)

## Doing

Design assignment done: consensus `docs/design/rpc-lane.md` and `docs/design/rpc-lane-model.py`
at consensus:dbf559b (pushed on quest/xapiand/consensus-rpc-lane). No consensus sources touched:
the quest is --after detent:consensus-transport and detent:consensus-driver.

## Next

1. Design review (lens `design`) of consensus:dbf559b; the lead answers Q1-Q4.
2. The lead forwards T1-T7 (the doc's "Requirements on consensus-transport") to Detent's lead.
3. After the driver and transport land: the doc's "First buildable steps" 1-4.

## Blocked on

The lead / owner: Q1 (where A4's Detent test runs; recommend this quest moves `Peers`' fetch onto
lane frames), Q2 (file set: add CMakeLists.txt, fuzz/fuzz_decoders.cc, test/transport/peers_test.cc,
docs/design/rpc-lane*, CHANGELOG.md, ARCHITECTURE.md), Q3 (the `fuzz` check configures without
CONSENSUS_FUZZ and libFuzzer, so `ctest -R rpc_fuzz` would pass unfuzzed), Q4 (A2's "same
connection": the lane is on Bulk, heartbeats on Control). Building waits on detent:consensus-transport.

## Evidence

| Criterion | Evidence |
|---|---|
| A1-A7 | none yet (design only); each has its check in the design's criteria table |
| model | job 261116-200021-283afb (design passes); mutants must fail: 261116-200036-5f5bed forget-is-violation, 200040-9732b6 grant-without-reserve, 200044-7ce605 no-release-on-close, 200047-65d447 no-regrant, 200051-577a89 grant-on-receive |

## Result

Design: lane on the Bulk connection; frames `DTRP0001` + kind/flags/word/call id (24 bytes) with
Call, Body, Credit, Cancel, Error and `end` as a flag; version range in the hello (DTHL0002);
per-call cumulative credit from a lane budget separate from group credit; retired ids discard late
frames; lane-only connections via hold/release; fetch becomes method 1 and DTFT0001 goes.

## Rulings I made

- R1 Lane on Bulk, not Control or a third connection (design D1).
- R2 "End" is a flag on Call/Body, not a sixth kind: one way to end a stream, one-frame calls.
- R3 Call ids by parity of the connection role (dialer odd), no direction bit.
- R4 Per-call windows only; no lane-wide connection window (the grantor owns the memory).
- R5 Overloaded calls are answered `Error overloaded`, never queued nor closing the connection.
- R6 The model lives beside the design as docs/design/rpc-lane-model.py (a design artifact, not
  consensus sources); the lead asked for one new file, the skill asks for an executable model.
