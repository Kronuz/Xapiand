# license-audit: Audit licenses and land notice actions

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:5caed332b (2026-11-03); its ledger `docs/status/license-audit.md` is the record. Its machine state is in `state.json`: `campaign status license-audit`.

## Outcome

The audit lists every license finding for the first MIT release, and the landed actions add ownership and third-party notices and install license documents with the binary. Three release blockers and the owner-reserved actions remain for Phase 8a.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] A1 Rewrite NOTICE as the third-party notice file: `docs/status/license-audit.md`
- A2 [artifact] A2 Rewrite copyright page to match: `docs/status/license-audit.md`
- A3 [artifact] A4 Normalize owner address in AUTHORS: `docs/status/license-audit.md`
- A4 [artifact] A5 Fix 20 copyright headers: `docs/status/license-audit.md`
- A5 [artifact] A6 Add headers to 17 source files and one test: `docs/status/license-audit.md`
- A6 [artifact] A7 Add headers or exemptions for tooling: `docs/status/license-audit.md`
- A7 [artifact] A8 Add headers to Dubalu-era headerless files: `docs/status/license-audit.md`
- A8 [artifact] A10 Name Apache-2.0 Python client in notices: `docs/status/license-audit.md`
- A9 [artifact] A11 Add bs58 MIT text: `docs/status/license-audit.md`
- A10 [artifact] A12 Add gmplot copyright and MIT text: `docs/status/license-audit.md`
- A11 [artifact] A13 Confirm y2j origin and restore notice: `docs/status/license-audit.md`
- A12 [artifact] A14 Add Apache header or rewrite benchmark CMake modules: `docs/status/license-audit.md`
- A13 [artifact] A15 Add KDE BSD notice: `docs/status/license-audit.md`
- A14 [artifact] A16 Confirm or delete CheckDIRSymbolExists chain: `docs/status/license-audit.md`
- A15 [artifact] A20 Record htm.png source: `docs/status/license-audit.md`
- A16 [artifact] A22 Keep Elasticsearch docs attribution: `docs/status/license-audit.md`
- A17 [artifact] A23 Move embedded notices into seven library LICENSE files: `docs/status/license-audit.md`
- A18 [artifact] A24 Set MIT license in docs package: `docs/status/license-audit.md`
- A19 [artifact] A25 Install LICENSE and NOTICE with binary: `docs/status/license-audit.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Split notice content between short `NOTICE` and generated `THIRD-PARTY-NOTICES`.
- Exempt listed data/config/generated tooling and keep owner-reserved actions untouched.
- Risk low: Audit and documentation actions; blockers remain unchanged (the lead's mapping).

## May land early

None.
