> Status: CURRENT (2026-10-27) sortable-codec workstream ledger; branch `design/sortable-codec`, local only.

# sortable-codec (design)

- Design: `docs/design/sortable-codec.md` revision 2 at this commit (hash recorded in the next); review: `docs/reviews/sortable-codec-design.md` (architect, preliminary, REJECT, 2026-10-27, Xapiand:57deb969c).
- Branch / worktree: `design/sortable-codec` at `~/code/Kronuz/Xapiand-wt/sortable-codec`; from Xapiand:6c5c1a40d; local only, never pushed.
- State: designing (ready for preliminary review)

## Done

1. Revision 0 (Xapiand:72f1006ac), the lead's rulings on Q1-Q4 (Xapiand:22418c6b8).
2. Revision 2, closing the architect's blocking items:
   1. L1: the layout is one tag per exponent for both signs (D2, D3); `model.py` rewritten to it. `selftest`: 213,624 values (boundaries at every exponent, 2^k + d of both signs, random over each type), 0 failures, longest 10 B; decoder fuzz of 400,000 random and mutated inputs, 219,996 accepted, 0 not canonical. `mutants`: three encoder mutants fail 15,617, 11,004 and 24,897 checks. `rules`: a witness for each D5 reject rule, six shown needed. The model's bytes and its accepted set equal the architect's `p4_alt.py` layout (`.scratch/sortable-codec-rev2/xcheck.py`: 0 of 196,961 typed values differ in bytes; 16,268,708 accepted inputs, 0 disagreements), so the byte layout the architect validated is unchanged.
   2. L2: D6 and T5 on the architect's 13 sets, both corpora and Xapiand's datetime buckets: smaller or equal in every set, values and terms (T5 prints PASS). Corpora: values 21,089 B against 22,790 B (7.5% smaller), terms 54,951 B against 57,601 B (4.6%); synthetic sets and the version estimate: values 170,070 B against 174,849 B (2.7%), terms 553,242 B against 581,663 B (4.9%). Snowflake ids 8.95 B against 8.99 B, hashes 8.96 B against 9.24 B (revision 0 was 22% and 18% larger)
   3. L3-L6 (step 2): D9 (the marker: write path per creation route, revision effect, check order, read-only opens), D10 (in-place upgrade, `.xapiand/*` at first start, what it carries, the manual route), D11 (protocol bumps, mixed versions, downgrades); rows T8, T13, T14; questions Q5-Q7 for the owner.
   4. C1: D7's API is one constrained template, `overloads.cc` compiles 20 call forms and rejects 7 (Apple clang 17, `-fsyntax-only`, clean). C2: T10's grep. C3: `max_finite` and `min_finite` (D7, D12). C4: `model.py rules` witnesses, T4 and step 1. C5: D5.

## Conditions (from the architect's review; they hold at the next gate)

- C1 (finding 7): the library's API takes every standard integer type without ambiguity on every platform, rejects `bool` and character types at compile time; T11 compiles it. In D7 (revision 2).
- C2 (finding 8): T10's grep filters by path prefix. In T10 (revision 2).
- C3 (finding 9): the missing-value sentinel becomes a named codec constant in step 2 (`sortable::max_finite`, D7).
- C4 (finding 10): a mutant per D5 reject rule, each caught by a unit test, in step 1's done list.
- C5: D5's arm64 `long double` question stays labelled unverified until someone runs `legacy_probe` on an arm64 Linux host; nothing in this design depends on its answer.

## Next

1. Preliminary review (architect). The byte layout is the one `p4_alt.py` validated; what is new for the architect is D9-D11 (marker, upgrade, protocol) and the size table.
2. After Q5-Q7 (Q1-Q4 are ruled): step 1 (the `sortable` library) by a developer; step 2 after `fix/numeric-range` lands.

## Blocked on

Q5 (a release older than this design on a new data directory), Q6 (`_version` restarts at 1 after the upgrade), Q7 (how a new node refuses an old one: a `Kronuz/cluster` change or a bus token), for the owner. Q7 (a) also needs Detent's agent. Step 1 is not blocked by any of them.

## Findings folded in

L1-L6 and C1-C5 of `docs/reviews/sortable-codec-design.md`, in the commit that carries revision 2 (this commit (hash recorded in the next)).

## Last verified

2026-10-27, text and Python only, no C++ build beyond the syntax-only `overloads.cc`; probes under `quiet light`.

## Log

- 2026-10-27: revision 0 drafted, Xapiand:72f1006ac.
- 2026-10-27: the lead's rulings on Q1-Q4, Xapiand:22418c6b8.
- 2026-10-27: architect's preliminary review, REJECT, Xapiand:57deb969c.
- 2026-10-27: revision 2 drafted, this commit (hash recorded in the next).
