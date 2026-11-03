> Status: CURRENT (2026-11-03) the ledger of Phase 8b's closing-benchmark design.

# closing-benchmark

- Design: docs/design/closing-benchmark.md (revision 0); reviews: none yet
- Branch / worktree: `design/closing-benchmark` at `~/code/Kronuz/Xapiand-wt/closing-benchmark` (local only, never pushed)
- State: designing (ready for preliminary review)
- Done criteria (set by the lead; contributors tick, never edit):
  - [ ] K1 (to be set by the lead)
- Done: the design draft, D1 to D12, T1 to T13, Q1 to Q6, first steps (this commit)
- Doing: nothing; stopped at ready for preliminary review
- Next: preliminary architect review; owner answers to Q1 to Q6; then first steps 1 to 5 (harness), the dry run on a `c7a.8xlarge`
- Blocked on: Q1 (X's build) blocks only X; Q2 (durability tier) blocks T1, T2, T6, T7 settings; Q4 (metal storage) blocks the metal lease. All the owner's.
- Findings folded in: none
- Design vs code: none (nothing built)
- Last verified: file:line citations checked against Xapiand master `432299ed0`, strata main `9d52824`, prospect main, 2026-11-03

## Log
- 2026-11-03: drafted revision 0. Found while reading: `DB_SYNCHRONOUS_WAL` is never set in `src/` (no durable-on-ack mode, Q2); the `v1.0.0-alpha.10` tag does not build on Linux (fixes on `xapiand`, Q1); `lease` launches a fixed 100 GB gp3 root (Q4).
