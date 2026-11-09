# postings-fuzz-memory: The postings fuzz target's memory growth

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status postings-fuzz-memory`.

## Outcome

prospect's `fuzz_postings` runs for a CPU-hour with its memory bounded: the growth road-to-release
lists before the release is attributed (the decoder, the target or libFuzzer's corpus) and fixed
where it lives, so the postings decoder, which reads users' disks, is fuzzed without the run
dying of memory. Repository: Kronuz/prospect.

## Acceptance criteria

- A1 The growth reproduced and attributed at a named prospect commit, with RSS over time (road-to-release, "Before release, also"): a `light` job
- A2 If the decoder allocates from untrusted lengths, it bounds them and a test with the crashing or growing input fails without the fix: the `build-test` check
- A3 One CPU-hour of `fuzz_postings` under ASan and UBSan with RSS flat after warm-up, 0 crashes: a `light` job
- A4 `fuzz_postings_replay` and the rest of prospect's suite pass: the `build-test` check

## Contracts

prospect's postings decoder keeps its format; only its bounds may change.

## Decisions

- Open (lead): road-to-release names "the postings fuzz target's memory growth" without a ledger or
  numbers; the target is prospect's `fuzz/fuzz_postings.cc` (Xapiand `docs/gates/phase7a.md`).
- Risk high: a decoder of on-disk bytes is a security surface (lens security).

## May land early

The reproducer and its test.
