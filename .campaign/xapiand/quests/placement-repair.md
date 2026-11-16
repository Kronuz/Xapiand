# placement-repair: Re-replication, drain, leader balance, rf change, root install

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status placement-repair`.

## Outcome

The root authority keeps every shard at its `rf` (design D7): a node `down` longer than
`--replace-after` (default 300 s) is replaced by learner, install, promote, remove; a drain moves a
node's replicas away by the same steps; `number_of_replicas` grows and shrinks through D7's steps,
the 0-to-1 growth window included; leaders are spread by `transfer_leadership`, which defers for a
group whose application reports an open multi-entry command, so balancing never aborts a chunked
upload (F26); a new root voter installs the root's records.

## Acceptance criteria

- A1 A node down past `--replace-after` is replaced with no acknowledged loss; an `rf = 1` group
  whose node is down waits for it (a simulation-style test and a job)
- A2 A drain empties a node, with no acknowledged loss and no shard below its `rf` at any step
  where a replacement exists (a job)
- A3 `number_of_replicas` 0, 1, 2, 1, 0 on a live group under writes, through D7's steps, with no
  acknowledged loss and digests equal after each step (a job)
- A4 Leader balancing defers a transfer while the group's application reports an open multi-entry
  command and performs it once the command ends; a mutant that transfers anyway aborts the
  command (`--fails-without`, a test with values)
- A5 A new root voter installs at 10^6 namespace records; the install time and bytes stated
  against D5's estimate of 2.5 s per 300 MB at 1 Gb/s (an `exclusive` job)
- A6 [artifact] `docs/mechanisms/placement-repair.md`

## Contracts

Provides `strata:placement-repair`: the replace, drain, `rf`-change and balance steps, and the
application hook a group exposes to defer leadership transfer. Consumes `strata:group-install` and
`strata:placement-records`.

## Decisions

- Design D7, F8 (the 0-to-1 window tolerates no failure, as the lone voter it grows from), F26
  (balancing waits for open chunked commands; the hook is generic, the chunked command is
  Xapiand's).
- Design section 5: track-c D9 gains D7's growth window (the lead's edit to `track-c`).

## May land early

Replace and drain (A1, A2) before the `rf` change and balancing.
