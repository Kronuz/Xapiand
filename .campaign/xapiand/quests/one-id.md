# one-id: A document's id is its only id; docids stay internal and dense

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status one-id`.

## Outcome

D84: a document's id is its only id. Strata assigns docids itself, in append order, so every shard's docids stay dense whatever ids users choose; Xapiand never derives a docid from an id (src/database/shard.cc:836-839, `new_did`) nor an id from a docid (the NumericId path, :831-833, src/database/handler.cc, schema.cc). Automatic numeric ids come from a per-shard counter, persisted with the shard, that keeps today's stride (shard s of n issues s+1, s+1+n, ...) and that explicit numeric ids advance, so the two never collide. Searches and GETs resolve documents by key. P1's cause goes away: the T4/T5 mix's PUTs at ids 11,000,001+ no longer land near docid 2.2M (Xapiand:a7b42914d, job 261113-030545-b38c8b: merged segments spanning ~540x their docs).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 After a RESTORE of ids 1..N and PUTs of explicit numeric ids far above them, then a forced merge, every shard's segments are contiguous (docids kind 0): a test reading the docids kinds that fails with today's `new_did` (`--fails-without` job)
- A2 Automatic numeric ids keep the stride and never collide with explicit ones, across a reopen (the counter persists) and with ids written before and after: tests with values, each with a mutant
- A3 No docid is derived from an id or an id from a docid anywhere in src/ (NumericId, numeric_docid and new_did gone or internal to the counter): [artifact] the design note `docs/design/one-id.md` names every former use and its replacement
- A4 The build-test check passes, CI's sanitizers green
- A5 x7-gate's T4/T5 rerun with fix (c) and lanes landed shows light p99 and heavy searches against master, a miss stated as a miss: one exclusive job of at most 10 minutes (D59)

## Contracts

Changes the on-disk meaning of a shard's docids and adds a persisted per-shard id counter: a pre-release format change (formats open until release, D78's precedent: no migration). Automatic numeric id values keep their stride and start above every numeric id present.

## Decisions

- Owner D84 (2026-11-13): one id.
- Lead: the counter keeps today's stride so ids stay unique across shards without coordination.

## May land early

None.
