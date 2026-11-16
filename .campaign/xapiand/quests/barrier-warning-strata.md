# barrier-warning-strata: strata pins completion with full-flush ordering and the barrier warning

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status barrier-warning-strata`.

## Outcome

strata pins the completion commit whose `PosixIOOptions` carries `full_flush_ordering` and
`barrier_warning`, so a host that builds its own IO (Xapiand does, in shard.cc and pool.cc) can turn
them on (D10, D16); no strata index option or callback. Repository: Kronuz/strata.

## Acceptance criteria

- A3 strata pins the completion commit of `barrier-warning-completion` on completion's main: the `build-test` check
- A4 prospect's suite passes against this strata (AGENTS.md "Verify a change"): an integration job
- A5 faultfs keeps a staged file's post-mark writes unordered: `stage()` snapshots the staged bytes and `crash()` treats later writes as unsynced before any protocol write after a barrier (`docs/reviews/commit-barriers-durability.md` C3, CB-5; Xapiand `docs/status/t11-barriers.md` follow-up 3, "Not done"): a faultfs test in the `build-test` check, with a mutant that keeps the post-mark write ordered killed
- A6 [artifact] The macOS TSan blind spot is recorded and concurrency fixes keep needing the Linux TSan job green (`docs/reviews/commit-barriers-durability.md` C2, CB-4, "the lead handles C2"): `docs/reviews/calibration.md`

## Contracts

Consumes `barrier-warning-completion`. Provides the index option and the warning callback to
Xapiand (`barrier-warning`).

## Decisions

- `--after barrier-warning-completion`.

## May land early

None.
