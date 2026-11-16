# group-install: Learners, install of segments and file sets, catch-up, membership, repair, witness

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status group-install`.

## Outcome

A `strata::group` grows and heals: a learner installs a checkpoint, the segment pack and the
application file set on one transfer (durability-modes D17's file sets, install only; design D16),
catches up from the log, and is promoted; voters are replaced and removed one step at a time; a
damaged segment or file fences its replica, which reinstalls at or below commit (C3). With
cluster-release Q3 (a), a witness voter stores the log, never applies, and leads only to hand off.
Install rides consensus's credited `SnapshotChannel` and fetch lane, so it lands after
`detent:consensus-fetch-macos`.

## Acceptance criteria

- A1 A learner installs a 64 KiB-chunked segment pack and a file set from a pinned manifest,
  catches up and is promoted; replacement and removal pass across leader changes; digests, file set
  included, equal the leader's (jobs)
- A2 A mutant install that skips the file set fails A1's digest (`--fails-without`); a retried
  install sends only the files the learner lacks or holds short, counted in bytes (a test with
  values)
- A3 A damaged segment, manifest or application file fences the replica, which reinstalls at or
  below commit and serves no corrupt byte (a test per file class)
- A4 Under the load that broke `detent:consensus-fetch-macos`, 100 installs of a 1 GiB file set on
  macOS end byte-equal with zero errors (a job)
- A5 With Q3 (a): the witness stores the log, never applies, leads only to hand off, and a
  two-data-voter-plus-witness group survives the loss of any one member (simulation-style tests)
- A6 [decision] cluster-release Q3, pending owner; lead default (a), `rf = 2 + witness`
- A7 [artifact] `docs/mechanisms/group-install.md`

## Contracts

Provides `strata:group-install`: the install transfer (segment pack plus file set, bytes on the
wire) and the membership steps (add learner, promote, remove, add witness). Consumes
`strata:group` and consensus's `SnapshotChannel` and fetch lane. `format-freeze` reviews the
transfer.

## Decisions

- Design D7 (short groups are one voter plus learners, never two voters), D16, F1, F9, F24 (no
  scrubber for the release: reads and installs fail closed).
- Lead: shares group-core's files; it starts when group-core lands.

## May land early

None.
