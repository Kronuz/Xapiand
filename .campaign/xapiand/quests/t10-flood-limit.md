# t10-flood-limit: T10's flood p99 limit scales with the host's cost per call

Risk: low (a harness). The lead writes this quest; contributors read it and never edit it.

## Outcome

T10's "light GET beside 16 clients flooding the per-call cap" row failed on FreeBSD CI at 110-123 ms
against a constant 100 ms set on the dev host. The exploration (`explore/t10-flood`,
docs/dev/t10-flood-limit.md) found the mechanism sound: on one reactor a light GET waits behind a
flood call that runs its whole ~22 ms per-call cap, and the CI VM's cost per call swings ~30% between
runs. The limit becomes 6x the median of nine flood calls timed alone on the same node.

## Acceptance criteria

- A1 [manual] FreeBSD CI's rows pass at the landing commit, and the run's T10 log shows the derived
  limit and its median (lead).
- A2 [artifact] `docs/dev/t10-flood-limit.md` states the derivation and the caveat that the per-call
  cap cases still cover a longer cap.

## Decisions

- Lead: a limit derived from a control measured in the same run, never a constant tuned to a host.

## May land early

None.
