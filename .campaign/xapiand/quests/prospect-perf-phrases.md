# prospect-perf-phrases: Phrase query drill, round 2

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:fc5dce7 (2026-10-26); its ledger `docs/status/perf-phrases.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-perf-phrases`.

## Outcome

Phrase queries recycle per-query objects, match two slots, bound TOP_k from counts and sum the position index (E1, E2, E3, E5 kept; E4 removed), with no format or disk change.

## Acceptance criteria

Carried from the ledger's target and Done list, each shown by the ledger as an artifact.

- A1 [artifact] Target: at most 1.0 of Tantivy on COUNT and TOP_10 for the 300 gate phrases: `docs/status/perf-phrases.md`
- A2 [artifact] E1, E2, E3, E5 kept and E4 removed, numbers in `docs/perf/phrases-explore.md` sections 4 to 7: `docs/status/perf-phrases.md`
- A3 [artifact] Review CR1 (sanitizers bypass the block stack) and CR2 (`test/recycled_test.cc`) folded in: `docs/status/perf-phrases.md`
- A4 [artifact] Measure the landing version on Linux, FreeBSD, arm64: `docs/status/perf-phrases.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The pair posting (+12% to +21% disk for COUNT parity) is undecided (section 7).
- CR3 (a delete after the thread stack's destruction keeps up to 8 blocks) is a follow-up.
- Risk normal: thread-local recycling and candidate skipping on the phrase path (the auditor's mapping).

## May land early

None.
