# perf-watch-peek: The keep-alive watch byte's extra read

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status perf-watch-peek`.

## Outcome

The extra read that the keep-alive watch byte costs per request in http (road-to-release, step
"Perf", gate for T12) is attributed and removed or bounded, with before and after numbers at named
commits, and Xapiand pins the http commit that carries it. Repository: Kronuz/http.

## Acceptance criteria

- A1 The cost per request of the watch byte's read, before and after, at named http commits, under an `exclusive` job, with an A/A control and the honest magnitude (road-to-release Perf, "performance engineer"): `bench/` A/B jobs
- A2 No behavior change: a client that disconnects or half-closes is still seen within one slice (Xapiand T9 depends on it): the `build-test` check (http's suite)
- A3 [artifact] Results in http's own docs (the file name is the lead's choice): `docs/perf/watch-peek.md`
- A4 [artifact] How the watch works after the change, if it changes: `docs/mechanisms/coroutine-handlers.md`

## Contracts

Consumes and may change http's `CancelWatch` (the peek a `cancel_token()` engages,
`docs/mechanisms/coroutine-handlers.md`); its observable behavior stays as documented there.
`x7-gate` A24 needs this landed and pinned.

## Decisions

- Open (lead): road-to-release names "the keep-alive watch byte's extra read" without a ledger; this
  quest reads it as http's `CancelWatch` peek (`recv` with `MSG_PEEK | MSG_DONTWAIT`,
  `docs/mechanisms/coroutine-handlers.md:81`). Confirm, or retarget the quest.
- The other half of Perf, sliced search's 1.4-2.2% one-slice cost, landed in prospect
  (prospect:5c98f81, `perf/sliced-residual`): main at 1.010 to 1.028 of the pre-sliced loop, the
  wall residual not attributed (prospect `docs/perf/sliced-search-residual.md`). Open (lead): is that
  enough before T12, or does this quest take it too.
- Prior evidence (audit, 2026-11-08), so it is not redone: L3's "known cost" (http `docs/status/phase7c-l3.md`:
  the one-byte watch read takes the next request's first byte) was changed twice after the L3 ledger.
  http:9f4c93f (2026-10-31, "Read ahead in the disconnect watch") and http:c37474a (2026-11-01, "Watch for a
  hang-up once the handler asks"): the watch now reads only once a handler asks about cancellation, through
  `CancelWatch`'s peek. http `CHANGELOG.md` "2026-11-01: the disconnect watch waits for the handler to ask":
  `bench_http` under `wrk`, i9-9980HK, START c23bb99 against this change, A/A 0.95 and 0.99: 87,316 to 93,657
  req/s at 8 connections, 97,986 to 98,730 at 64 (CPU per request 11.1 to 10.6 us, 10.2 to 10.1 us), and
  `readahead_test` [L], [Q], [N] with mutants. http `docs/perf/t6-t7-2026-11-03.md` (Linux metal) has the W2a
  gate rows on the task loop. Open (lead): whether these already discharge A1 and A4, leaving only the
  `exclusive` A/B at the head Xapiand pins.
- Assignment: performance (strong tier); measurements follow `references/measurement.md`.

## May land early

Benchmarks and tests that change no library code.
