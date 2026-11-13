# sortable-encoding: A clean-room number encoding replaces sortable_serialise

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status sortable-encoding`.

## Outcome

Every number Xapiand sorts, ranges or looks up by term is encoded by MIT code: the
`Kronuz/sortable` codec in C++ and a clean-room Python port in the client with a cross-language
identity test; ids (exact-id-keys's branch, Xapiand:58a14faf6, its first step) and values switch
together, with no compatibility with earlier bytes (pre-release, D87); both GPL files and the GPL Python module are gone (Phase 8a's first item;
`docs/design/sortable-encoding.md`, branch `design/sortable-encoding`; license-audit blockers 1
and 2).

## Acceptance criteria

- A1 [decision] The owner's Q1-Q3 answers: D87
- A2 T1 order: `memcmp` agrees with an exact comparator on every adjacent pair of any type mix; 0 failures (design T1): `sortable_property` in the `build-test` check
- A3 T3 C++ equals Python: the client's `encode` reproduces all 113,491 lines of `vectors.txt`, and 10^6 seeded random inputs give identical bytes; `decode` accepts and rejects the same inputs (design T3): a job
- A4 T5 same answers: docs E2E assertions all pass and bodies equal the pre-switch run except raw-byte fields; every sort and range answer on the real corpus equals the pre-switch answer or brute-force truth (design T5): jobs
- A5 T6 exact at the old failures: range, sort and min/max equal brute force on the listed edge values; 0 wrong (design T6): `harness/numeric_range_check.py` extended, a job
- A6 T7 size and cost on master, before and after, honest magnitude either way, and the restore rate that prices the migration (design T7, the lead's): `exclusive` jobs
- A7 Fast: encode and decode cost per value at or below the old codec on `sizes.py`'s real-value set, and indexing, sort and range throughput on the real corpus at or above master's, outside the A/A band only if faster (owner D87: it must not become a bottleneck): `exclusive` jobs at named commits
- A8 T9 no GPL: `git grep -n sortable_serialise -- src contrib` prints nothing, both GPL files and the Python module removed, the check added to `harness/seam_check.sh`; ARCHITECTURE and the license note updated (design T9; license-audit A3): the `no-gpl` check
- A9 T10 platforms: T1, T2's CI part, T3 and the unit tests pass on every CI platform and sanitizer job (design T10): CI
- A10 [artifact] How it works: `docs/mechanisms/value-encoding.md`
- A11 [artifact] Changelog `[Unreleased]`: exact numeric order for ids and values: `docs/src/content/docs/changelog.md`

## Contracts

Consumes `sortable-proof`'s codec bytes (sortable:67b2a90). Provides the on-disk value and term
encoding of the release; `docs-upgrade-page` and
`closing-benchmark` measure and document it. Freezing this encoding for the release is the
owner's (PLAN open question 8).

## Decisions

- The architect's preliminary review of the design (new for a reviewer: D1's case against the 8-byte map, D7, D8, T3, T5) is the `design` lens, before step 3.
- Critical: it replaces bytes on users' disks and freezes them for the release; owner approval
  and a design review come before the switch (step 3). Steps 1 and 2 (the library's proof, one
  seam, no byte change) may start before Q1.
- On Q1 (a), the lead relabels `docs/design/sortable-codec.md` (branch `design/sortable-codec`,
  never landed, REJECTed preliminary review) SUPERSEDED; that branch is not adopted.
- Clean room: nobody on this quest opens `src/sortable_serialise.{cc,h}` or the GPL Python module.

## May land early

Step 2: the seam (every direct call behind `Serialise::`/`Unserialise::`), which changes no stored
byte.

- Owner, 11-13 (D87): Q1 the variable codec, and it must be fast; Q2 Python names that read well, no
  aliases for old ones; Q3 confirmed (400 for NaN and out-of-range integers, -0.0 indexes as 0). No
  format marker and no compatibility code: pre-release bytes change freely. Docs and new names use
  American spelling (serializer).
- Lead, 11-13: exact-id-keys folds in; its files join this set at start.

- Lead, 11-10 (D52): the switch also updates the docs pages that describe value encoding and the About page's licence sentence (written by docs-site-refresh against the old encoding); they join the file set once docs-site-refresh lands.

- Lead, 11-10 (D55): the changelog edit also fixes line 40's promise that a 2xx write survives a crash (it survives a stop; D13, D54).
