# docs-upgrade-page: Upgrading from alpha.9 and alpha.10 data

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-upgrade-page`.

## Outcome

The docs site has an upgrade page that takes a user from an alpha.9 or alpha.10 data directory
(Xapian indexes, which the release refuses to start on) to the release: dump on the old binary,
restore on the new one, and what changes on the way (road-to-release, "Before release, also").

## Acceptance criteria

- A1 [artifact] The upgrade page, from alpha.9 and alpha.10 data: `docs/src/content/docs/upgrading.md` (the file name is the lead's choice)
- A2 Its examples run: the dump and restore round trip of a real alpha.10 data directory gives the same documents (`harness/cli_roundtrip_check.sh` across the two binaries): a job
- A3 The page names every user-facing change a restored index shows, from `docs/status/behavior-changes-audit.md` and the value encoding (`sortable-encoding`'s changelog line): the docs E2E (`e2e` check) and the reviewer's read
- A4 Docs E2E passes with the page in the collection: the `e2e` check

## Contracts

Consumes the release's refusal of Xapian data (Xapiand:d71cf7a97, "refuse to start on Xapian
data") and `sortable-encoding`'s format marker.

## Decisions

- `--after sortable-encoding`: the page describes the final value encoding.

## May land early

None.
