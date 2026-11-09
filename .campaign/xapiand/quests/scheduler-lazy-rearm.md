# scheduler-lazy-rearm: Debouncer lazy re-arm and schedule_result

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at scheduler:2cbea95 (2026-10-20); its ledger `docs/status/perf-lazy-rearm.md` in Kronuz/scheduler is the record. The quest's repository is `../scheduler`. Its machine state is in `state.json`: `campaign status scheduler-lazy-rearm`.

## Outcome

The Debouncer re-arms lazily, so 1,000 hot keys hold 1,002 tasks instead of 507,000, and every `add`, `debounce` and `delayed_debounce` is `noexcept` and returns `schedule_result { scheduled, past_horizon, stopping, no_memory }` with no silent drop.

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] `bench/bench_churn.cc`, the re-arm and `test_debouncer_rearm`; docs `docs/perf/lazy-rearm.md`: `docs/status/perf-lazy-rearm.md`
- A2 [artifact] Churn A/B, three rounds: 507,000 tasks and 99-108 MB to 1,002 and 1.5 MB on 1,000 hot keys: `docs/status/perf-lazy-rearm.md`
- A3 [artifact] TSan and ASan clean; CI run 39382526110 green on 8 jobs: `docs/status/perf-lazy-rearm.md`
- A4 [artifact] Owner's follow-up: `schedule_result` from every add; refusals tested, each failing with its fix reverted: `docs/status/perf-lazy-rearm.md`
- A5 [artifact] Downstream: Xapiand's 17 TUs build against the pins; logger `fix/refused-add` acts on refusals: `docs/status/perf-lazy-rearm.md`
- A6 [artifact] Review C1 and CR2 fixed (`test_debouncer_touch_after_finish`, horizon refusal keeps the pending call): `docs/status/perf-lazy-rearm.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner: past_horizon is an error and is never run now; `stopping` and `no_memory` still run the call.
- Residual: a key pending when `finish()` began and never touched again is not run; Xapiand's pool close commits writables.
- Risk high: lock order and dropped work in the debouncer (the auditor's mapping).

## May land early

None.
