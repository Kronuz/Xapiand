> Status: CURRENT (2026-11-02) the X4 security re-check's conditions (X4-R1 to R4) in progress on `phase7c/x4-r1`; not pushed.

# phase7c X4-R1 (build): close the re-check's conditions

Branch `phase7c/x4-r1` in `Xapiand-wt/x4-r1`, from master `Xapiand:10ab134ad`. Source: `docs/reviews/phase7c-x4-security.md`, "Re-check 1"; mechanism `docs/mechanisms/lua-bounds.md`; design D10, T10.

## State

In progress. Code, tests and docs for R1, R3 and R4 are written; proofs are being recorded below.

## Done criteria

- [ ] R1 per-call cap (`Limits::max_call_work`, 16 x 10^6 units): code, unit test, mutant, live flood and hold numbers, drift-tested patch.
- [ ] R3 tests that bite: `error({})` handlers, print budget, 1 s foreign revalidation.
- [ ] R4/R2 docs: changelog, `scripting.md`, `lua-bounds.md`, D10/T10, `patches.md`.
- [ ] Proof: Release and ASan+UBSan unit tests, TSan on the Lua tests, docs E2E 641 of 641, `harness/lua_bounds.py`.

## Done

(commits listed below as they land)

## Next

Mutants, sanitizers, E2E, harness, live numbers.

## Blocked on

Nothing.
