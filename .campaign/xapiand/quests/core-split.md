# core-split: Separate core and search build targets

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:3f88a15 (2026-10-20); its ledger `docs/status/core-split.md` is the record. Its machine state is in `state.json`: `campaign status core-split`.

## Outcome

Core headers and callers form an independently buildable core target, while search code resides behind its own target and headers. Core key lookup works with stored sections and is tested independently.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Headers moved into include/strata/core and include/strata/search; callers rewritten by name: `docs/status/core-split.md`
- A2 [artifact] Six mixed files split at the plugin seam; basic_index<document_layout> instantiates with core headers alone: `docs/status/core-split.md`
- A3 [artifact] core_boundary, core_boundary_negative, and core_kv_test added: `docs/status/core-split.md`
- A4 [artifact] Design note, AGENTS.md map, and KV harness requirements documented: `docs/status/core-split.md`
- A5 [artifact] basic_index::get added for layouts with stored and checked in core_kv_test and index_test: `docs/status/core-split.md`
- A6 [artifact] Main merge ports the KV harness to strata::core; Release and ASan+UBSan ctest 42/42: `docs/status/core-split.md`
- A7 [artifact] Review CR1-CR3 and CN5/CN6 fixed; Release and ASan+UBSan ctest 45/45: `docs/status/core-split.md`
- A8 [artifact] CR4 remains open until the final tip is green on all 8 jobs: `docs/status/core-split.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Review: CR1-CR3 and CN5/CN6 fixes recorded in docs/reviews/core-split-code.md; verdict not stated in the ledger.
- CR4: land only after the final tip's run is green on all 8 jobs.
- Risk high: shared public APIs and library boundaries (the lead's mapping).

## May land early

None.
