# phase7c-x4-r1: Bound Lua call work

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:8ef0f8633 (2026-11-02); its ledger `docs/status/phase7c-x4-r1.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x4-r1`.

## Outcome

A per-call work cap and tighter matcher limit bound uninterruptible Lua work. Regression tests, documentation, and flood probes capture the review conditions and their results.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] R1 Condemn calls once work exceeds max_call_work; cap matcher steps and charge string.rep repetitions: `docs/status/phase7c-x4-r1.md`
- A2 [artifact] R3 Add tests for close-handler shapes, logged print bytes, and foreign-script revalidation: `docs/status/phase7c-x4-r1.md`
- A3 [artifact] R4, R2 Document stack, conversion, per-call, and re-arming bounds in changelog, scripting guide, mechanism note, and design: `docs/status/phase7c-x4-r1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Matcher step cap is lua_xcallwork() / 5: 3.2 x 10^6.
- Keep the limits patch and its pinned hash together.
- D10 and T10 state the 20 ms bound.
- Risk high: Lua execution limits and security surface (the lead's mapping).

## May land early

None.
