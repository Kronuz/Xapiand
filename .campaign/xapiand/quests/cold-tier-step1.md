# cold-tier-step1: Segment records and lazy mapping

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:66ddb16 (2026-10-27); its ledger `docs/status/cold-tier-step1.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step1`.

## Outcome

Segment records install bodies with compare-exchange, and `segment_view::seg()` exposes the installed segment. D9 map failures preserve host-error behavior and mark damaged segments for healing.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Record/body install and loser ownership; TSan and mutants are recorded as stand-ins for T6: `docs/status/cold-tier-step1.md`
- A2 [artifact] K2 D9 map-point failure rules, fault-injected tests, healing behavior and swallowed-round count: `docs/status/cold-tier-step1.md`
- A3 [artifact] K4 (part item 1 touches) Missing or resized manifest-named files fail open: `docs/status/cold-tier-step1.md`
- A4 [artifact] K7 (listing only) prospect construction sites and Xapiand seam are listed with the pin bump: `docs/status/cold-tier-step1.md`
- A5 [artifact] K8 (part this branch can show) ASan, UBSan and TSan clean; CI green on every platform remains with the lead: `docs/status/cold-tier-step1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The eager-segments default remains on; `STRATA_EAGER_SEGMENTS=0` disables it for a test run.
- `seg()` returns a pointer; Xapiand has no adoption site in its local master.
- CR3 remains open for `merge_scratch::bytes()` host errors.
- Risk high: Concurrent record installation and mapping failure rules affect index integrity (the lead's mapping).

## May land early

None.
