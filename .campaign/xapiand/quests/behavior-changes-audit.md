# behavior-changes-audit: Audit behavior changes since alpha.10

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4b37d3a67 (2026-11-03); its ledger `docs/status/behavior-changes-audit.md` is the record. Its machine state is in `state.json`: `campaign status behavior-changes-audit`.

## Outcome

The audit catalogs 49 user-facing behavior changes since v1.0.0-alpha.10 and identifies their recorded decision makers and risk levels. It flags seven agent-decided medium- or high-risk changes for owner review and documents changelog gaps.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Audit 49 user-facing behavior changes since v1.0.0-alpha.10: `docs/status/behavior-changes-audit.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner, reviewer/architect, or agent attribution recorded per audited change.
- Risk low: Read-only audit and documentation research (the lead's mapping).

## May land early

None.
