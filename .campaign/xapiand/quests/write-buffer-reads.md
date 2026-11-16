# write-buffer-reads: Shared reads of write buffers

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:2f56d64 (2026-10-26); its ledger `docs/status/write-buffer-reads.md` is the record. Its machine state is in `state.json`: `campaign status write-buffer-reads`.

## Outcome

Readers can query published buffer views at a position while a writer continues applying transactions. Reads account for key and docid predecessors, pending deletes, and scans over active and frozen buffers.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] I4-1 Records use the specified D3 byte layout; existing producers reserve fields by operation type: `docs/status/write-buffer-reads.md`
- A2 [artifact] I4-2 Docid maps build only under the lease; readers use a const probe without building: `docs/status/write-buffer-reads.md`
- A3 [artifact] I4-3 Grown key and docid maps retire under D10's generation stamp; the first docid-map build is inline under the lease: `docs/status/write-buffer-reads.md`
- A4 [artifact] I4-4 First build and growth of active docid maps, and frozen-buffer builds, pass under readers: `docs/status/write-buffer-reads.md`
- A5 [artifact] I4-5 D6 schedules cover keyed deletes, key-changing replacements, rewrites, both keys, and reads around freeze and merge install: `docs/status/write-buffer-reads.md`
- A6 [artifact] I4-6 Named docid-check, predecessor-walk, and immediate-free mutants are killed: `docs/status/write-buffer-reads.md`
- A7 [artifact] I4-7 The D6 first-docid-only skip is set before V and restored on rejection: `docs/status/write-buffer-reads.md`
- A8 [artifact] I4-8 Item 4 model-check schedules pass under TSan and ASan with every item 4 mutant killed: `docs/status/write-buffer-reads.md`
- A9 [artifact] I4-9 Core suites pass; flushed bytes match the items 2-3 base harness: `docs/status/write-buffer-reads.md`
- A10 [artifact] I4-10 Memory per buffered record and per open index is reported against the base: `docs/status/write-buffer-reads.md`
- A11 [artifact] I4-11 CI green on the branch: `docs/status/write-buffer-reads.md`
- A12 [artifact] I4-12 Index-owned reads run on the lease holder's thread; other-thread reads use views at a position: `docs/status/write-buffer-reads.md`
- A13 [artifact] I4-13 Const docid probing, generation retirement, fixed position-index directory, and non-building `has_docid()`: `docs/status/write-buffer-reads.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- 2026-10-26 lead: large docid-map builds and scan-run sorts stay inline under the lease at step 1; D12's pool rule moves to step 3.
- CR1: rejected records stay in the lane while readers may hold views; their bytes count toward buffered memory and flush thresholds.
- CR2: select-32 is tested by ordered-vector comparisons over 216 cases.
- CR3: index-owned reads are restricted to the lease holder; off-thread reads use views.
- Risk high: Adds cross-thread visibility, reader views, and generation-based reclamation (the lead's mapping).

## May land early

None.
