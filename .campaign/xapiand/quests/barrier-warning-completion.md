# barrier-warning-completion: PosixIO warns when a barrier is only an ordering point

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status barrier-warning-completion`.

## Outcome

completion's `PosixIO` emits a warning, once per volume, when it runs on storage where the
ordering barrier is not a durable flush (non-Apple storage on macOS), and exposes the opt-in
full-flush ordering that strata wires through (owner 2026-10-29, D10; lead 2026-10-30, D16).
Repository: Kronuz/completion.

## Acceptance criteria

- A1 The warning fires once per volume where the barrier orders without flushing, and never on Linux or FreeBSD (D10, D16): a test through the sync-call seam (`perf/commit-barriers`) in the `build-test` check
- A2 The full-flush ordering option, when on, issues the full flush instead of the barrier: a test of the exact call sequence
- A3 [artifact] The docs state that the barrier orders and is not a flush, guaranteed only on Apple SSDs (t11-barriers C1 / CB-3): `docs/mechanisms/barriers.md` (the file name is the lead's choice)

## Contracts

Provides the warning hook and the `full_flush_ordering` option to strata (`barrier-warning-strata`).

Keeps `SyncFiles`, `OrderFiles` and `PreparationFilePolicy::Stage` with their signatures and default
sequences unchanged: Detent's `namespace-s1-s2` pins completion:560c288 and consumes them (Detent's lead,
2026-11-08, xapiand msg-1). The new warning and `full_flush_ordering` are additive and opt-in. Any change to
those three is messaged to Detent (`campaign message detent: ...`) before landing.

## Decisions

- "Not a priority" (lead, 2026-10-30); before the release.
- Open (lead): how a library warns (a callback the host logs, or a counter) is this quest's design
  call; completion's checkout `../completion` must exist beside the home.

## May land early

Tests of the existing barrier behavior.
