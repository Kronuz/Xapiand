> Status: CURRENT (2026-10-29) ledger of the `phase7a/t11-barriers` workstream (performance engineer); not landed.

# Ledger: T11 commit barriers

## State

Gate 7a T11 CPU row on the gate's estimator: 0.372x Xapian (pairs 0.355 to 0.408) on the landing heads, against 0.465x (A/A 0.478x) for master's code in the same session (an earlier grid on the pre-fix heads: 0.396x against 0.489x). Three experiments, no loser, nothing landed. Branches are pushed for completion, strata and prospect; Xapiand's branch is local.
Waiting for the architect's scoped durability check (the ordering argument is in `docs/perf/t11-barriers/README.md`).

## Done

- E1 fold the first directory barrier into the data batch: completion `cdf1cb3`, strata `0765e28`.
- E2 order the manifest temporary with a barrier: completion `5a338d8`, strata `307d08b` (superseded by E3, kept in history as the weaker-assumption fallback).
- strata faultfs models ordering points: strata `67671ec`.
- E3 one ordering point and one flush: completion `fdfb94f`, strata `d01a1c4`.
- Cleanup, docs and pins: completion `3665ee7`, strata `bbfb986` `cccb2f9`, prospect `23207cc`, Xapiand pin commit `d153d95f3`.
- TSan race fix (the commit copied a whole segment ref while a merge read it): strata `e471404`, prospect `b9b3c02`, Xapiand pin `d7d98d74e`.
- Measurements, raw data, probe: `docs/perf/t11-barriers/`.

## Next

1. Architect's scoped durability check of `order_files_and_directory` and the reordered `commit_files` (state now changes after the durable commit).
2. Lead: land completion, then strata, prospect, Xapiand pins; rerun gate 7a T11 on `main`.
3. Unmeasured: macOS arm64 (probe and bulk), Linux and FreeBSD (unchanged by construction); power loss on hardware.
4. Optional: investigate the +3% peak RSS (3 runs per arm).

## Blocked on

Nothing.
