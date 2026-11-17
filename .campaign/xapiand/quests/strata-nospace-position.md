# strata-nospace-position: A refused apply never moves strata's position

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-nospace-position`.

## Outcome

A `no_space` from the commit-allowance check (`include/strata/core/index.hh:860` at strata
491a1bdc) still moves the index's position (`:878-880`), so a retry of the same entry is answered
`skipped` and a replica silently loses a committed write (group-core design re-check B7). Every
refusal that a retry can clear (`no_space`, `flush_owed`, any environment-dependent result) leaves
the position where it was; only semantic refusals (D3's list) move it. The empty "mark"
transaction group-core's capture uses is covered too (review N11).

## Acceptance criteria

- A1 A test with values: an entry refused `no_space` and retried after space frees is applied, not
  `skipped`, and the position moves exactly once; the same for `flush_owed`; a mutant that moves
  the position on `no_space` fails it (`--fails-without`)
- A2 The empty mark transaction at a full allowance answers a retryable refusal and leaves the
  position (a test)
- A3 The group-core reviewer's probe `.scratch/review-group-core-design/verdict_position_probe.py`
  (job 261117-025140-4bbcdd exits 1 at 491a1bdc) exits 0 at the fix (a job)
- A4 Every refusal the index returns is listed as position-moving or not in the header's API doc,
  matching D3 (the size-cap `invalid`, review N12, decided there)

## Decisions

- Lead: fix the cause in strata core rather than fencing every replica on `no_space`.

## May land early

Nothing: the change is small.
