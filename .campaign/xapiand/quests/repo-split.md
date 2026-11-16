# repo-split: Move search into prospect repository

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:426cc59 (2026-10-20); its ledger `docs/status/repo-split.md` is the record. Its machine state is in `state.json`: `campaign status repo-split`.

## Outcome

Strata retains the core and its core tests, while prospect contains the search library, search tests, tools, and related documentation. Indexes built by strata and prospect are byte-identical in the four reported comparisons.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Search files and history moved to prospect; strata retains the core and shared documents: `docs/status/repo-split.md`
- A2 [artifact] Prospect history check: 455 of strata's 713 commits carry over: `docs/status/repo-split.md`
- A3 [artifact] Identity comparison matches across the four reported runs: `docs/status/repo-split.md`
- A4 [artifact] Strata and prospect test suites pass locally, totaling 45 on macOS: `docs/status/repo-split.md`
- A5 [artifact] Both repository CI runs were in progress when recorded: `docs/status/repo-split.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead's 2026-10-20 request: move the search side into Kronuz/prospect with history.
- Land prospect main first, then strata with a merge commit, then the catalog branch.
- Risk high: shared public API and repository boundary (the lead's mapping).

## May land early

None.
