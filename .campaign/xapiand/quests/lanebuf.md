# lanebuf: Write-buffer bake-off research

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:18febff (2026-10-30); its ledger `docs/status/lanebuf.md` is the record. Its machine state is in `state.json`: `campaign status lanebuf`.

## Outcome

The bake-off compares buffer approaches and scan-tail experiments, with C's scan tail selected for further work. The prototype was rebuilt and timed, but the branch is not merged.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `a4aa2c4` rebuilds the prototype on the hazard registry, position index, fixed chunk directory, scan-triggered builds, component epoch, real segments, and applier path: `docs/status/lanebuf.md`
- A2 [artifact] Runner, adapters, bytes mode, prebuilt RocksDB clones, JSON fix, and E1 behind `LANEBUF_E` are landed in the prototype: `docs/status/lanebuf.md`
- A3 [artifact] Results, verdicts, scan-tail plan, and raw data are recorded: `docs/status/lanebuf.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner accepted C on 2026-10-26; the write buffer is built on it.
- E1, E4, E5, E6, and E7 show wins; E2 is removed and E3 is kept after E5.
- Lead must decide whether C's 1.5 to 1.8x p99.9 gap at full write speed closes T3.
- Risk low: Prototype experiments and measurements; no engine change landed here (the lead's mapping).

## May land early

None.
