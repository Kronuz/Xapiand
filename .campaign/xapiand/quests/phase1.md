# phase1: Phase 1, foundation libraries

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:6a3cddde (2026-10-11), the commit that records the architect's approval of the Phase 1 gate. No ledger existed then: strata's `docs/PLAN.md` "Phase 1", `docs/gates/phase-1.md` and `docs/reviews/phase-1.md` are the record. The libraries live in their own repositories (Kronuz/bitpack, fst, docset, task, residency, hashes, varint). Its machine state is in `state.json`: `campaign status phase1`.

## Outcome

The foundation libraries exist as standalone public repositories with tests, a benchmark, README, ARCHITECTURE and AGENTS files, and their APIs are frozen at the gate: `bitpack`, `fst`, `docset`, `task`, `residency` with a synchronous backend, strata's scorer concept, and CRC-32C. Their decoders are memory-safe on corrupt input, fuzzed and pinned by golden fixtures, and every benchmark target passes except `fst`'s full-vocabulary lookup.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 1: foundation libraries": the APIs frozen at the gate, the tests, and the benchmark targets, each shown by the gate report as an artifact.

- A1 [artifact] `bitpack`: FOR encode and decode of a 128-integer block at a given bit width, delta decode with a base, PFOR with exceptions, VByte tails, the block header, and the no-wdf and all-ones flags, frozen at the gate: `docs/gates/phase-1.md`
- A2 [artifact] `fst`: sorted-input `Builder::insert(key, output)` and `finish()`; `get`, `floor`, range streams, `intersect(automaton)`, frozen at the gate: `docs/gates/phase-1.md`
- A3 [artifact] `docset`: the iterator concept (`doc`, `next`, `advance`, `cost`); dense and sparse sets; in-place AND, OR, ANDNOT; cardinality, frozen at the gate: `docs/gates/phase-1.md`
- A4 [artifact] `task`: `task<T>`, the scheduler and I/O concepts, yield with a budget, `when_all`, cancellation, and an Asio adapter in a separate target, frozen at the gate: `docs/gates/phase-1.md`
- A5 [artifact] The scorer iterator concept and the residency interface with a synchronous backend: `docs/gates/phase-1.md`
- A6 [artifact] CRC32C in its own library, `crc32c`, hardware-accelerated (SSE4.2, ARMv8), moved from `journal/codec.h`: `docs/gates/phase-1.md`
- A7 [artifact] Tests: round-trips for every bit width from 0 to 32 at every block position: `docs/gates/phase-1.md`
- A8 [artifact] Tests: automaton intersection against brute force on 100,000 random patterns: `docs/gates/phase-1.md`
- A9 [artifact] Tests: `task` executor affinity, cancellation at yield, exception propagation, 10^6-deep symmetric transfer in constant stack, and 10^8 tasks with random cancellation under ASan, UBSan and TSan: `docs/gates/phase-1.md`
- A10 [artifact] Tails: `varint` tail size and decode speed against LEB128 and StreamVByte on the Wikipedia postings tails; a smaller encoding lands in `varint`: `docs/gates/phase-1.md`
- A11 [artifact] `bitpack`: decode with delta at least 90% of a reference SIMD-BP128/D1 and at least 3x scalar; encoded size within 2% of Lucene's on the same postings: `docs/gates/phase-1.md`
- A12 [artifact] `fst`: lookup p50 within 2x an open-addressing hash map on the Wikipedia vocabulary at at most 25% of its bytes; build rate and bytes per term reported against a front-coded sorted array: `docs/gates/phase-1.md`
- A13 [artifact] `docset`: within 1.2x of CRoaring on AND, OR, ANDNOT and iteration for densities 10^-6 to 0.5: `docs/gates/phase-1.md`
- A14 [artifact] `task`: create, await and destroy of a no-op task at most 30 ns with zero `malloc`; yield-and-resume through the Asio binding at most 1 µs p99 on an idle loop: `docs/gates/phase-1.md`
- A15 [artifact] Every gate: decoders memory-safe on corrupt input, one CPU-hour of fuzzing each with zero findings, golden fixtures, pinned dependencies: `docs/gates/phase-1.md`

## Contracts

The frozen APIs of A1 to A5, in the libraries' own repositories at the commits `docs/reviews/phase-1.md` names (re-check: bitpack 6d14df1, docset 4e4d73f, fst 2cb8927, task c3c30e4, residency e52bd7b, varint de3d05d).

## Decisions

- Phase 1 review (2026-10-11): not approved on B1 to B4 (corrupt input, fuzzing, fixtures, pins); the re-check approved the gate the same day, every test rebuilt under ASan and UBSan.
- The review's rulings carry rows into Phase 2: the `bitpack` size row becomes exact once Phase 2 writes skip data; `fst`'s full-vocabulary lookup miss is accepted for the block-index use the architecture specifies.
- Risk high: frozen public APIs and decoders of untrusted bytes (this audit's mapping).

## May land early

None.
