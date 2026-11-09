# x3-1-chunked-registry: Chunked lock-free reader registry

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:80e8f5a (2026-11-02); its ledger `docs/status/x3-1-chunked-registry.md` is the record. Its machine state is in `state.json`: `campaign status x3-1-chunked-registry`.

## Outcome

The reader registry grows in chunks and supports lock-free acquire through a cached hint and a single link CAS. Indexes create their own registry by default; chunks remain allocated while any record is claimed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Revision 4 text (R4.1 to R4.8) and its D4, D5, D9, D10 edits are complete: `docs/status/x3-1-chunked-registry.md`
- A2 [artifact] `basic_reader_registry` uses four-record chunks; empty until first acquire; capacity and unbounded modes work: `docs/status/x3-1-chunked-registry.md`
- A3 [artifact] Litmus reruns with K = 1 and new growth shapes: 10^7 iterations, 0 free-while-read: `docs/status/x3-1-chunked-registry.md`
- A4 [artifact] Hazard tests cover chunk capacity, stable records, reuse, appenders, and growth stress: `docs/status/x3-1-chunked-registry.md`
- A5 [artifact] ASan+UBSan and TSan pass for the six listed binaries: `docs/status/x3-1-chunked-registry.md`
- A6 [artifact] All named chunked-registry mutants are killed: `docs/status/x3-1-chunked-registry.md`
- A7 [artifact] Registry benchmark compares acquire/release and owner scans: `docs/status/x3-1-chunked-registry.md`
- A8 [artifact] mac preflight passes; Linux and FreeBSD VMs are not run; CI jobs not watched: `docs/status/x3-1-chunked-registry.md`
- A9 [artifact] Architect check L1, C1, and C2 is incorporated; sanitizer and mutant reruns pass: `docs/status/x3-1-chunked-registry.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Architect rules `claimed() const noexcept` walks existing chunks without allocating; `slot()` is test-only and allocating.
- Architect folds in one TLS hint and records the 4096-record capacity budget.
- A relaxed-load mutant for the claimed check survives TSan; acquire ordering is argued, not killed.
- Risk high: Changes lock-free reader registration and concurrent reclamation (the lead's mapping).

## May land early

None.
