# format13: Format 13 landing and pre-freeze work

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:76fa331 (2026-10-20); its ledger `docs/status/format13.md` is the record. Its machine state is in `state.json`: `campaign status format13`.

## Outcome

Format 13 adds configurable structures, key handling, and manifest metadata while preserving format-specific validation and compatibility rules. The recorded landing branch remains pre-freeze, with required review and freeze work still open.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1: T1 cases pass on all eight CI jobs: `docs/status/format13.md`
- A2 [artifact] K2: Every decoder F3 adds has a fuzz section selector and checked-in corpus replayed under ASan and UBSan: `docs/status/format13.md`
- A3 [artifact] K3: F22 and format-12 refusal fixtures pinned and byte-identical on all eight jobs; reorder merge hash equal at reorder_threads 1 and 8: `docs/status/format13.md`
- A4 [artifact] K4: T9 lifecycle tests pass over format 13 for crash points, digest schedules, disk-full rows, keyed/keyless, and document profile: `docs/status/format13.md`
- A5 [artifact] K5: Full suite passes under ASan, UBSan, and TSan on CI at every cited commit: `docs/status/format13.md`
- A6 [artifact] K6: media_test sweep covers bytes 0 to 15 of every segment; transient flips heal and sticky flips fail as damage: `docs/status/format13.md`
- A7 [artifact] K7: Each decision's tests cite its F number in the adding commit: `docs/status/format13.md`
- A8 [artifact] K8: Step 7c changes digest in exactly one commit, re-pinning digest.txt; all other commits leave it unchanged: `docs/status/format13.md`
- A9 [artifact] K9: Fresh open touches header page and one run of pages ending at footer, not lengths: `docs/status/format13.md`
- A10 [artifact] K10: Key path refuses key-prefixed terms; check, query, scan, verify, and parity agree with model: `docs/status/format13.md`
- A11 [artifact] K11: F20 checks reject unsupported masks/version ranges and invalid segments/manifests: `docs/status/format13.md`
- A12 [artifact] K12: Freeze evidence for F1 items 2 to 6 recorded in docs/perf under granted holds: `docs/status/format13.md`
- A13 [artifact] K13: F13-R18 fixed in rebase plan; F13-R25 items included in design before named commits: `docs/status/format13.md`
- A14 [artifact] K14: Points support up to 16 bytes per dimension; freeze report baseline raw files copied into docs/perf: `docs/status/format13.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-18: Q7 yes; PISA reference applies to carried rows that close in format 13.
- Lead's rulings include F13-R44 and F13-R45; H22 verdict and PLAN sentence remain open.
- Risk high: format and persistent data migration (the lead's mapping).

## May land early

None.
