> Status: CURRENT (2026-11-03) sortable-encoding workstream ledger; branch `design/sortable-encoding`, local only.

# sortable-encoding (design)

- Design: `docs/design/sortable-encoding.md` (Phase 8a's first item: replace `sortable_serialise`, C++ and the Python client's port).
- Branch / worktree: `design/sortable-encoding` at `~/code/Kronuz/Xapiand-wt/sortable-encoding`, from Xapiand:432299ed0; local only, never pushed.
- State: designing (ready for preliminary review)

## Done

1. Callers read (26 direct calls in 9 files, the `Serialise::` users, the Python client's exports); the four places where integer, positive and floating meet in one order recorded with file:line.
2. Size study on real data (`docs/design/sortable-encoding/sizes.py`, output `sizes.txt`): 430,166 numbers, variable 4.25 B per value; fixed 8 B is 1.88x per value, 1.26x per distinct value, 1.32x per distinct term. Byte counts only, no hold.
3. Draft: D1-D8, T1-T10, Q1-Q3, three first steps. Adopts `Kronuz/sortable`'s bytes (sortable:67b2a90); the Python client port and its cross-language test (T3) added at the lead's request.

## Next

1. Preliminary review (architect, sent by the lead). New for a reviewer: D1's case against the 8-byte map, D7 (the Python port), D8 (the generation marker), T3 and T5.
2. Steps 1 and 2 can start now; step 3 waits for Q1.
3. On Q1 (a), the lead relabels `docs/design/sortable-codec.md` (branch `design/sortable-codec`) SUPERSEDED.

## Blocked on

Q1 (width: the variable codec against the plan's "8-byte map"), Q2 (the Python client's exported names), Q3 (confirm the carried NaN, -0.0 and integer-overflow rulings), for the owner. Steps 1 and 2 are not blocked.

## Last verified

2026-11-03: text and Python only; no build.

## Log

- 2026-11-03: worktree and ledger created from Xapiand:432299ed0.
- 2026-11-03: size study run; draft written, Xapiand:e04fdf092; ready for preliminary review.
