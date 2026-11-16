# release: The rename and the first Xapian-free release

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status release`.

## Outcome

The project has its new name across the repository, binary, CMake project, Homebrew formula,
documentation site and `XAPIAND_*` identifiers, with the old name as a redirect and the HTTP API's
routes unchanged; the changelog's release section is cut, the tag is pushed, the Homebrew bottles
and the docs site are published (road-to-release 8c; strata `docs/PLAN.md` Phase 8).

## Acceptance criteria

- A1 [decision] The owner picks the name after a trademark search (road-to-release 8c; PLAN open question 1): a Decisions row
- A2 The rename is complete: no `xapiand`/`XAPIAND_` identifier left outside the redirect, the changelog and docs history; the binary, CMake project and formula carry the new name (strata `docs/PLAN.md` Phase 8): a `git grep` check and the `build-test` check
- A3 The HTTP API keeps its routes: the docs E2E passes unchanged against the renamed binary: the `e2e` check
- A4 [manual] The old name redirects (repository, docs site, formula) (strata `docs/PLAN.md` Phase 8): the owner checks after publishing
- A5 [artifact] The changelog's `[Unreleased]` becomes the release section, `PACKAGE_VERSION` matches the tag (`AGENTS.md`, "Cutting vX.Y.Z"): `docs/src/content/docs/changelog.md`
- A6 A27: the license audit re-run at the release commit finds no blocker (license audit A27): a job and the `license-closure` audit
- A7 The Phase 8 gate on the release commit: a clean build without Xapian, every suite green (strata `docs/PLAN.md` Phase 8): the milestone's validation on master
- A8 CI, the bottles and the docs site's workflows are green on the tag: CI

## Contracts

Renames every public identifier but the HTTP routes; the `XAPIAND_*` environment names keep working
through the redirect for a stated period (the owner's call in A1's row).

## Decisions

- Critical: the rename and the published release are irreversible; the owner approves the name and
  the release, and a design review covers the rename's plan before it is built.
- `--after` every other Phase 8 quest: the tag is the last landing.
- D39, D40: also after `release-rows`, `format-freeze` and `more-like-this`. `release-rows` ran at the
  release's pins, or re-runs at them; the release notes state 7c's T6 (major reactor faults at twice
  RAM), which gates at Phase 6 (D15).
- Pushing the tag and publishing are the owner's to authorize.

## May land early

None.
