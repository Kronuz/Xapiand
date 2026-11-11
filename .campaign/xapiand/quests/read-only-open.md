# read-only-open: A search opens an index with no lock and no flush; the first write upgrades

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status read-only-open`.

## Outcome

An open for search takes no lock and runs no flush (D60). The first write on that handle upgrades it: it takes the lock and runs the syncs a writable open runs now, before its first commit can be acknowledged. A crash at any point leaves the index as durable as today's open.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] A design note fixes the upgrade: what a read-only open may observe (a manifest the log has not yet synced), the order of lock, directory sync and log sync at upgrade, and what concurrent readers see: `docs/design/read-only-open.md`
- A2 A read-only open runs no fsync or F_FULLFSYNC and takes no lock, shown by a test counting syncs through strata's file hooks: a job
- A3 The first write after a read-only open upgrades before its commit is acknowledged; a crash between the upgrade's steps leaves the index readable with every acknowledged commit, under the crash model: a job
- A4 Opens for search cost no device flush: open latency under the catalog_tiers load before and after, one exclusive A/B job of at most 10 minutes (D59)

## Contracts

Changes strata's index open contract: a handle opened for search is read-only until its first write.

## Decisions

- Lead, 11-11: the owner chose this (D60); a design review (lens design) rules A1 before building. The crash model is the gate evidence for A3 (D57's durability exception).

## May land early

The design note (A1) after its review. Nothing that changes open's behaviour.
