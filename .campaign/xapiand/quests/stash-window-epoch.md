# stash-window-epoch: stash window handshake and fail-fast add

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at stash:351ef2e (2026-10-20); its ledger `docs/status/perf-window-epoch.md` in Kronuz/stash is the record. The quest's repository is `../stash`. Its machine state is in `state.json`: `campaign status stash-window-epoch`.

## Outcome

stash's far producers register against a fixed-width window epoch, `add` is `noexcept` and returns `stash_result { placed, past_horizon, no_memory }`, a failure after the slot claim terminates, and `horizon(ctx)` is public.

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] Owner's decisions folded in (d234d24): fixed window width, compile-time hooks only, `add` result, `noexcept` next: `docs/status/perf-window-epoch.md`
- A2 [artifact] Model on the fixed-width variant: no violation in 6 configurations; every mutant still loses: `docs/status/perf-window-epoch.md`
- A3 [artifact] TSan and ASan+UBSan on `test.cc` and `concurrent.cc` (lead 0 and 60, 5 runs each): clean: `docs/status/perf-window-epoch.md`
- A4 [artifact] Starved stress, 40 interleaved runs: 0 failed, 0 lost: `docs/status/perf-window-epoch.md`
- A5 [artifact] A/B against main, 5 rounds under a strict hold: no difference beyond the spread: `docs/status/perf-window-epoch.md`
- A6 [artifact] DW11 fail-fast with tests, model results, sanitizers; CAS claim measured and replaced: `docs/status/perf-window-epoch.md`
- A7 [artifact] Review C3 and CR2 fixed: traces before the leaf claim (`stash_trace_throw`), public `horizon(ctx)`: `docs/status/perf-window-epoch.md`
- A8 [artifact] A timed run on Linux and arm64: `docs/status/perf-window-epoch.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner chose fail-fast over the CAS claim's cost (up to 16.7% slower on one hot leaf).
- Risk high: lock-free concurrency in the timer queue (the auditor's mapping).

## May land early

None.
