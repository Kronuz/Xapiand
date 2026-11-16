> Status: CURRENT (2026-11-16) sortable-encoding workstream ledger; quest branch `quest/xapiand/sortable-encoding`.

# sortable-encoding (design)

- Design: `docs/design/sortable-encoding.md` (Phase 8a's first item: replace `sortable_serialise`, C++ and the Python client's port).
- Branch / worktree: `design/sortable-encoding` at `~/code/Kronuz/Xapiand-wt/sortable-encoding`, from Xapiand:432299ed0; local only, never pushed.
- State: implementing; the design's D87 revision awaits the design reviewer's confirmation

## Done

1. Callers read (26 direct calls in 9 files, the `Serialize::` users, the Python client's exports); the four places where integer, positive and floating meet in one order recorded with file:line.
2. Size study on real data (`docs/design/sortable-encoding/sizes.py`, output `sizes.txt`): 430,166 numbers, variable 4.25 B per value; fixed 8 B is 1.88x per value, 1.26x per distinct value, 1.32x per distinct term. Byte counts only, no hold.
3. Draft: D1-D8, T1-T10, Q1-Q3, three first steps. Adopts `Kronuz/sortable`'s bytes (sortable:67b2a90); the Python client port and its cross-language test (T3) added at the lead's request.

## Next

1. Design reviewer confirms the D87 revision (no marker: F1, F8-F10 obsolete; F2's CTest home; F3-F7, F11, F12 folded in).
2. Steps 1 and 2 (CTests, the Python port, the seam), no byte change; step 3 after the confirmation.

## Blocked on

Nothing for steps 1 and 2. Owner answered Q1-Q3 (D87).

## Last verified

2026-11-03: text and Python only; no build.

## Log

- 2026-11-03: worktree and ledger created from Xapiand:432299ed0.
- 2026-11-03: size study run; draft written, Xapiand:e04fdf092; ready for preliminary review.
- 2026-11-16: design revised for D87 (no format marker or compatibility; T1 and T3 homed in Xapiand's CTest).
