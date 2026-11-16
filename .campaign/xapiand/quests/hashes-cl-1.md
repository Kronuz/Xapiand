# hashes-cl-1: CRC-32C leaves hashes for its own library (CL-1)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at hashes:1543e21 (2026-10-20); its ledger `docs/status/cl-1.md` in Kronuz/hashes is the record. The quest's repository is `../hashes`. Its machine state is in `state.json`: `campaign status hashes-cl-1`.

## Outcome

CRC-32C is Kronuz/crc32c (`<crc32c/crc32c.hh>`, target `crc32c::crc32c`, no dependencies, with its history); hashes no longer carries it and nothing forwards (consolidation item CL-1).

## Acceptance criteria

Carried from the ledger's State and Done, each shown by the ledger as an artifact.

- A1 [artifact] `crc32c.hh`, its test, benchmark and docs removed from hashes; README, ARCHITECTURE, AGENTS, API and BENCHMARKS updated: `docs/status/cl-1.md`
- A2 [artifact] hashes' suite passes; a project taking hashes and journal configures and runs in both declaration orders: `docs/status/cl-1.md`
- A3 [artifact] Detent's closure configures with journal on the Detent replay branch: `docs/status/cl-1.md`
- A4 [artifact] CI run 39421533539 green on all eight jobs: `docs/status/cl-1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The owner chose a library over the leaf design (HASHES_FAMILIES, then `hashes_crc32c`); commits 233254b and 512a20b on `cl-1-crc32c-leaf` are that superseded design and stay unmerged.
- Landing order: crc32c, then hashes, then journal, strata, residency, then the catalog; Detent bumps its own pins (journal's `cl-1-detent-replay` is its replay).
- Risk normal: moves a checksum other libraries persist (the auditor's mapping).

## May land early

None.
