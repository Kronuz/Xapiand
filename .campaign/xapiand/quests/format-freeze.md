# format-freeze: The freeze-readiness review before the first release

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status format-freeze`.

## Outcome

Every on-disk format the first Xapian-free release writes is reviewed for freezing before real data
lives in it, as D30 requires: strata's files, prospect's segment format and Xapiand's own files.
The review states each format's version, the rows its landing left open, its fixtures and its
fuzzing; then the owner freezes what the release writes, or keeps a format pre-freeze with the reason
and the upgrade path. Repository: Kronuz/strata, whose `docs/PLAN.md` owns the formats.

## Acceptance criteria

From D30 and strata `docs/PLAN.md` ("Every gate"; formats stay open until the first release).

- A1 [artifact] The freeze-readiness review: every format the release writes, with its version, its
  open rows (format 13's K1 to K5, K7, K10, K12 and K14; `prospect-f13-query`'s F10 deviations), its
  fixtures, its fuzzing (prospect's from `release-rows` A5) and a recommendation per format:
  `docs/reviews/freeze-readiness.md`
- A2 Every decoder of strata's formats that the release reads: one CPU-hour of fuzzing with zero
  findings; readers reject unknown versions, with a fixture per format: jobs
- A3 [decision] The owner freezes the formats the release writes, or keeps one pre-freeze with the
  reason and the upgrade path (reserved: the strata format freeze for the release): a row in
  PLAN.md's Decisions

## Contracts

Freezes what A3 approves: from then on a change to a frozen format needs a new version and a reader
for the old one.

## Decisions

- D30: one freeze-readiness review before the first release that real data lives in. The first
  Xapian-free release is that release: users reindex into it (behavior-changes audit #1).
- D39: the release waits on it.
- Lands after `sortable-encoding`, the last planned format change before the release (its T8 adds a
  per-shard marker); a later format change reopens A1 for that format.
- The architect reviews A1; the owner decides A3.

## May land early

The fuzz targets and fixtures (A2).
