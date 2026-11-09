# gate7a: Gate 7a, Xapiand on strata at rf = 1

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:0d194335b (2026-10-29), the commit that closes the gate's re-check notes after the architect's re-check closed L1, L2 and C1 to C4. The gate (step S5 of `docs/design/xapiand-on-strata.md` section 4) had no ledger of its own: `docs/gates/phase7a.md` and strata's `docs/reviews/phase-7a-gate.md` are the record; its fix rounds are `gate7a-fixes`, `phrase`, `t11-barriers`, `gate7a-l1` and `gate7a-l2`. Its machine state is in `state.json`: `campaign status gate7a`.

## Outcome

Xapiand serves from strata behind the seam at `rf = 1` with Xapiand's WAL: the docs E2E passes with zero regressions against the Xapian baseline, every body difference is listed and justified, a `kill -9` replays to the same document set, a dump moves between engines unchanged, concurrent index and search is clean under TSan, and strata's engine share per document is at most half of Xapian's in CPU and on the critical path.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 7" (7a) and "Every gate", and the rows T6 to T11 of `docs/design/xapiand-on-strata.md` section 5, each shown by the gate report as an artifact.

- A1 [artifact] T6 E2E on strata: zero assertion regressions against the Xapian baseline, single node, the engine forced by the environment override: `docs/gates/phase7a.md`
- A2 [artifact] T7 bodies: every body difference between engines listed and justified: `docs/gates/phase7a.md`
- A3 [artifact] T8 crash recovery: after `kill -9` during indexing, restart replays the WAL to the same document set as a Xapian run; the strata digest check passes: `docs/gates/phase7a.md`
- A4 [artifact] T9 migration: `_dump` on one engine, `_restore` on the other, equal `_dump` output: `docs/gates/phase7a.md`
- A5 [artifact] T10 concurrency: concurrent index and search on one strata shard clean under ThreadSanitizer: `docs/gates/phase7a.md`
- A6 [artifact] T11 replacements win by a lot: strata's engine share per document at most half of Xapian's, in CPU and on the critical path; parity fails (owner, 2026-10-27, D6): `docs/gates/phase7a.md`
- A7 [artifact] Every gate: tests under ASan, UBSan and TSan; one CPU-hour of fuzzing for each decoder and parser added or changed; readers reject unknown format versions; coverage measured: `docs/gates/phase7a.md`
- A8 [artifact] Every gate: the phase's test-parity rows, the scale statement, the uncovered lines' reasons and the other platforms' sanitizer runs: `docs/gates/phase7a.md`
- A9 [artifact] The architect's gate review approves (strata `docs/reviews/phase-7a-gate.md`): `docs/gates/phase7a.md`

## Contracts

None recorded: landed work; the strata engine is in `src/engine/strata/`, and its rulings (R-P2, R-RM) in `docs/design/xapiand-on-strata.md`.

## Decisions

- First run on Xapiand:9fa890ecc (2026-10-28): T6 and T11 fail, T7 lists 41 differences; fix round `gate7a-fixes`, the positional phrase (`phrase`), T11 at 0.47x (Xapiand:8f6332a01) and one flush per commit (`t11-barriers`).
- Gate review (strata `docs/reviews/phase-7a-gate.md`, 2026-10-29): REJECT on F3 (refused features, against R-RM) and F6 (every-gate checks unaccounted); the re-check closes L1, L2 and C1 to C4 with no new blocking finding.
- The "Not run here" items carry with an owner and a landing point: Linux sanitizer runs and the scale statement to the 7b gate or the CI job, RapidJSON's patch to Phase 8 (quest `phase7b` and `x7-gate` A15 carry them).
- Risk high: the engine swap under every user request, the WAL and concurrency (this audit's mapping).

## May land early

None.
