# license-closure: An MIT source and binary release

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status license-closure`.

## Outcome

Nothing in Xapiand's tree or binary blocks an MIT release: the Lenna images are replaced by a
generated sample, the owner's reserved actions of the audit are done, no Xapian reference is left,
and the audit covers every dependency, RapidJSON's patch included (road-to-release 8a;
`docs/status/license-audit.md`, blocker 3 and actions A3, A9, A17 to A19, A21, A26).

## Acceptance criteria

- A1 Blocker 3: `docs/public/assets/Lenna.*` is gone; the store-api examples use a generated sample image (license audit; Xapiand:e63910117 on `fix/license-actions`): the `e2e` check
- A2 [artifact] A3: README, ARCHITECTURE and AGENTS stop calling `sortable_serialise` GPL once it is gone (license audit A3): `ARCHITECTURE.md`
- A3 [decision] A9: `docs/discussions` carries a license, the same in all three copies (owner's action): a Decisions row
- A4 [decision] A17, A18: the stopword lists and `mime.types` are recorded or regenerated (owner's actions): a Decisions row
- A5 [decision] A19, A21: the Google Maps screenshot kept or redrawn; `generator.py`'s phrase tables rewritten (owner's actions): a Decisions row
- A6 [decision] A26: the package license metadata (owner's action): a Decisions row
- A7 No Xapian reference left in `src/`, `contrib/` and the build outside the changelog and docs history (road-to-release 8a; strata `docs/PLAN.md` Phase 8: `xapian.h`, the `pack.h` include): the `no-xapian` check
- A8 [artifact] The audit's dependency table covers RapidJSON's patch (road-to-release 8a): `docs/status/license-audit.md`
- A9 Release build and unit tests pass; `cmake --install` stages `LICENSE`, `NOTICE` and `THIRD-PARTY-NOTICES` (license-audit ledger, Build and test): the `build-test` check

## Contracts

The installed notice files and the package metadata the release (`release`) ships.

## Decisions

- Adopts `fix/license-actions` (Xapiand:c47a49bd6): four of its five commits landed on master as
  `phase8/license-actions` (patch-equivalent), so a rebase leaves only Xapiand:e63910117 (the
  generated sample image). The audit's other 19 actions landed (history quest `license-audit`).
- Blocker 1 (`sortable_serialise`) and blocker 2 (the Python port) close in `sortable-encoding`;
  A2 waits for it (`--after sortable-encoding`).
- A27 (re-run the audit at the release commit) belongs to `release`.
- Review: a security reviewer for the audit (road-to-release 8a).

## May land early

A1 (the sample image), which changes no code.
