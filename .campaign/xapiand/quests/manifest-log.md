# manifest-log: Commit metadata as an append-only manifest log

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status manifest-log`.

## Outcome

An index's durable state, apart from its segment files, is one append-only file per index: a
snapshot at generation g, then one edit per commit appended in one write with one barrier, rolled
over by snapshot when the log grows past its bounds, the old log and obsolete segments leaving
through the reclaim queue lazily (owner 2026-11-04, D25; strata `docs/design/manifest-log.md`
revision 1 on `design/manifest-log`, first step 3; steps 1 and 2 landed as `manifest-log-steps12`).
Repository: Kronuz/strata.

## Acceptance criteria

- A1 Step 3: the log writer (one write per append, the truncate-back, one record in flight); `commit_files` appends the edit or rolls over by D7; `recover` replays and syncs nothing; `retain` keeps generations by D8; `check()` replays the log against memory; the live-docs, metadata and field files retire (design first step 3): tests in the `build-test` check
- A2 T4's cases, T6's oracle and `media_test` updated and passing (design first step 3): the `build-test` check
- A3 SR2-strata: `apply()` costs time in proportion to the edit (a name set, an identity index, a delete edit O(k log n)) and D7's bounds are enforced on read; T7's replay-cost row includes the 20M-doc, 10M-deleted shape (8.9 s for 128 one-ordinal delete edits today) (steps12 ledger, step 3's conditions): a test with a time bound and the T7 job
- A4 SR3: deletes decode within a bound of their bytes (a bitset decodes to 64x its bytes today); the representation is the owner's call with step 3's deleted structure (steps12 ledger): a test and the fuzz target
- A5 SR4: the open and the writer drop the state on any exception from `apply()`, not only `corrupt_index` (steps12 ledger): an allocation-failure test
- A6 D6.3's caller rules: a scan that stops inside the first record is damage whatever the tail verdict, and the snapshot's generation equals the log's g (steps12 ledger): tests
- A7 Crash images: every honest crash image of the log reads as torn or whole, 0 misread; the single-bit-flip property holds (steps12 ledger, step 1's criteria carried to the log): tests
- A8 [decision] The lead measures T1 to T3, T5, T7 to T9 on main after landing (design first step 4) as milestone checks, per D57
- A9 Xapiand's and prospect's suites pass on this strata (AGENTS.md "Verify a change"): integration jobs
- A10 [decision] The owner's Q1 (the manifest log alone, or with the standalone turn log), Q2 (`retained_predecessors` protects less), Q3 (a lost write of the last edit reads as torn) (design "Questions for the owner"): a Decisions row

## Contracts

Changes strata's on-disk commit metadata (the manifest becomes `manifest-<g>`); `pre_freeze`
moves. Consumes journal's framing component (landed in journal, pinned by strata:4d61aa76).

## Decisions

- Reviews are lenses, not criteria: the architect's re-check of revision 1 (the review REJECTed first step 3 until L1 to L3 fold in; `docs/reviews/manifest-log-design.md`) is the `design` lens before step 3 builds, and the security reviewer's decoder-safety review of step 2's reader is the `security` lens.
- Not on road-to-release's list: live strata work the earlier lead drove on 2026-11-04 under the
  owner's ruling D25. Open (lead): keep it in this campaign's strata lane, or move it out.
- Lenses: design (the re-check, before step 3), durability (the commit protocol) and security (the decoder).
- Adopts `design/manifest-log` (strata:79beaf4, revision 1 and its re-check) as the quest branch.
- K2 (the standalone turn log) is the design's step 5 and waits for Q1.

## May land early

The design documents; codec tests with no behavior change.
