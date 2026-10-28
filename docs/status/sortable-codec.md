> Status: CURRENT (2026-10-27) sortable-codec workstream ledger; branch `design/sortable-codec`, local only.

# sortable-codec (design)

Branch `design/sortable-codec` from Xapiand:6c5c1a40d, worktree `~/code/Kronuz/Xapiand-wt/sortable-codec`.
Design: `docs/design/sortable-codec.md`. Not pushed.

## State

Designing (ready for preliminary review). Revision 0 drafted: decisions D1-D10, gate rows T1-T12, open
questions Q1-Q4, steps 1-3. Nothing built beyond the reference model and the black-box probe.

## Done

1. Reference model `docs/design/sortable-codec/model.py`: self-test 211,526 values (boundaries at every
   exponent, 2^k + d both signs, random over each type), 0 failures; three mutants (no 1 - f for negatives,
   no fraction bit in the direct tag, no fraction bit in the escape field) fail 179,601, 63,722 and 108,254;
   strict decoder fuzzed with 400,000 random and mutated inputs, 137,421 accepted, every one re-encodes to
   itself.
2. Sizes (`sizes.py`, old codec built from Xapiand:6c5c1a40d as a black box): values 36,230 B against
   38,445 B, accuracy terms 109,069 B against 114,634 B on accounts and cities plus synthetic datetimes.
3. Black-box facts: old -0.0 and 0 are `80`; old +inf is 17 bytes of `ff`, -inf the empty string; NaN
   asserts (debug build).

## Next

1. Preliminary review (architect: format and D9 migration); fold findings in.
2. After Q1-Q4: step 1 (the `sortable` library) by a developer.

## Blocked on

Q1 (NaN at indexing), Q2 (-0.0), Q3 (library home; blocks where step 1 lives), Q4 (`uint64` >= 2^63 in an
integer field), for the owner.

## Log

- 2026-10-27: revision 0 drafted (commit in `git log` of this branch).
