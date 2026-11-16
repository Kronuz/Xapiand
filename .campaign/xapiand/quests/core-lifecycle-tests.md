# core-lifecycle-tests: Port lifecycle tests to document layout

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:8713d2b (2026-10-20); its ledger `docs/status/core-lifecycle-tests.md` is the record. Its machine state is in `state.json`: `campaign status core-lifecycle-tests`.

## Outcome

The core's document layout has lifecycle, reclaim, directory, media, and background-merge test coverage. The tests exercise core headers and strata::core without relying on search.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Six lifecycle programs, four mapped runs, directory_tmpfs, and shared helpers ported to document layout: `docs/status/core-lifecycle-tests.md`
- A2 [artifact] Mutants for media, reclaim, directory, lifecycle, background_merge, and index are detected: `docs/status/core-lifecycle-tests.md`
- A3 [artifact] 19 tests pass locally in RelWithDebInfo, ASan+UBSan, and TSan: `docs/status/core-lifecycle-tests.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-20: port lifecycle suites to document_layout and drop only search-specific assertions.
- Lead lands after CI is green.
- Risk low: tests and tooling only (the lead's mapping).

## May land early

None.
