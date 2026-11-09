# wbuf-inline: The write buffer's records hold their key and stored bytes inline

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status wbuf-inline`.

## Outcome

strata's write buffer keeps a record's key and stored bytes inline in the lane for every layout
that opts in, one shape with no switch, bounded by its chunk, with the lane's chunks charged to the
budget and grown by a 32nd of the lane from 4 KiB to 1 MiB after a 1 KiB first (ruling (c), D26):
1.07-1.67x main's records per flush, and readrandom a tie with main. Repository: Kronuz/strata
(`docs/status/wbuf-inline.md` on `perf/wbuf-inline`).

## Acceptance criteria

- A1 CR1 (review): every append goes through `record::emplace`, inline only within a chunk, else a whole record; `append_n` throws past a chunk in every build (`docs/reviews/wbuf-inline-code.md`, re-check CLOSED at strata:7791520): `long_key_records` and `record_strides` in the `build-test` check, mutants m1 and m2 killed
- A2 CR2 (review): inline is opt-in (`inline_kind`, `inline_layout()`); `add_stored_view` refuses at compile time a layout whose builder it would skip (re-check CLOSED): `fields_test` (`own_builder_parts`), mutants m4 and m5
- A3 The lane charge: the buffer charges the lane's chunks plus what whole records hold beyond it; the `doc_bytes - stride` underflow is impossible (`static_assert` and the push's `assert`) (ledger Done 5, strata:8b1de41): `lane_charged` in the `build-test` check
- A4 Chunk growth by ruling (c): `wbuf::next_chunk` 1 KiB first, then a 32nd of the lane, 4 KiB to 1 MiB; revert and mutants M1 to M3 killed (ledger, chunk round, strata:c439e69): `lane_growth`, `chunk_limit_room`, `pressure_flush_bounded`, `defer_io_test`'s identity (163 mutations, digest `0x6cc0ff118bf776d3`)
- A5 Release, ASan+UBSan and TSan ctest pass on macOS, with no sanitizer report (ledger: 69 of 69 each at strata:c439e69): the `build-test`, `asan` and `tsan` checks at the landed commit
- A6 Linux: ASan+UBSan passes (ledger: 70 of 70); `resource_budget`'s 1-in-300 flake on main is not this change (ledger, chunk round): CI on the landed commit
- A7 prospect's suite passes against this branch with prospect's companion change (prospect:c0c4260, 51 of 51, churn and dump identical at flush threads 1 and 2) (ledger; review NEW-1): an integration job with `--ref prospect=../prospect@test/wbuf-inline-flushes`
- A8 Records per flush against main at every share, digests identical across arms (ledger share sweep: 1.07-1.67x; 8 segments against main's 11 in `pressure_flush_bounded`): the `wbufrr` A/B as a `strict` job at the landed commit
- A9 [artifact] Results: `docs/perf/wbuf-inline.md`
- A10 [artifact] How it works, with "Design vs code": `docs/mechanisms/write-buffer.md`
- A11 The FreeBSD failure of `write_buffer` at strata:9ce248e (CI run 41944174997, attempt 1, job `ci / freebsd-x86_64`: `test/write_buffer_test.cc:506` `CHECK(index_test_access::merges_done(ix))` and `:516` `CHECK(fenced)`; green only on attempt 2) is explained and fixed, or shown on main without this branch: a repeat job of `write_buffer_test` (at least 300 runs) on the FreeBSD VM at the landed commit, and on main for the comparison

## Contracts

The write buffer's record layout in memory and the budget's charge; no on-disk byte changes
(digests identical across arms). prospect's `test/index_test.cc` budget case moves with the flush
points (`wbuf-inline-prospect`).

## Decisions

- The code review (`docs/reviews/wbuf-inline-code.md`: REJECT at strata:6f26aa2, re-check CLOSED
  CR1 and CR2 at strata:7791520) is history: it saw neither strata:8b1de41 (the lane charge) nor
  strata:c439e69 (chunk growth). The new correctness review is scoped to strata:7791520..9ce248e.
- CI at strata:9ce248e (run 41944174997, 2026-11-05) is green only on its second attempt: the first
  failed FreeBSD's `write_buffer`, the test this branch changes, and the earlier lead stopped at the
  quota limit while reading that failure ("not a known flake"; the 1-in-300 flake the chunk round
  reproduced on main is `resource_budget`'s). A11 carries it. Then a rebase onto main (the branch is
  84 commits behind; main changed `index.hh` since).
- Open (architect), not blocking this landing: values past about 990 B stay whole records, a
  second shape at a common size (ledger Blocked on 1; no saving at 1500 B values, 1660.7-1666.3
  B/record as main); layouts carrying more than the stored bytes keep whole records (Blocked on 3;
  the review ruled (a)).

## May land early

None.
