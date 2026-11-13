# one-id: A document's id is its only id; docids stay internal and dense

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status one-id`.

## Outcome

D84: a document's id is its only id. Strata assigns docids itself, in append order, so every shard's docids stay dense whatever ids users choose; Xapiand never derives a docid from an id (src/database/shard.cc:836-839, `new_did`) nor an id from a docid (the NumericId path, :831-833, src/database/handler.cc, schema.cc). Ids route to one of B fixed buckets (D85, B = 1024): numeric id N to bucket (N-1) % B, other ids by key hash % B; a table in the index metadata maps buckets to shards, so a later reshard moves whole buckets and never changes an id. Automatic numeric ids come from a counter per bucket, persisted with the shard owning it, issuing b+1, b+1+B, ...; explicit numeric ids and restored ids advance it, so the two never collide. Searches and GETs resolve documents by key. P1's cause goes away: the T4/T5 mix's PUTs at ids 11,000,001+ no longer land near docid 2.2M (Xapiand:a7b42914d, job 261113-030545-b38c8b: merged segments spanning ~540x their docs).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 After a RESTORE of ids 1..N and PUTs of explicit numeric ids far above them, then a forced merge, every shard's segments are contiguous (docids kind 0): a test reading the docids kinds that fails with today's `new_did` (`--fails-without` job)
- A2 Automatic numeric ids keep the stride and never collide with explicit ones, across a reopen (the counter persists) and with ids written before and after: tests with values, each with a mutant
- A7 Routing is by bucket, never by shard count: an explicit numeric id lands in the shard owning bucket (N-1) % B, and an automatic id is issued only from a bucket its shard owns: tests with values over n = 1, 3 and 5 shards, and a mutant routing numeric ids by key hash
- A6 A dump and RESTORE round trip keeps ids: documents with explicit ids (strings and numbers) and automatic numeric ids, dumped and restored into a fresh index, come back with every `_id` and body unchanged, and the next automatic id is above every restored numeric id, across a reopen: a test with values and a mutant (the restore not advancing the counter)
- A3 No docid is derived from an id or an id from a docid anywhere in src/ (NumericId, numeric_docid and new_did gone or internal to the counter): [artifact] the design note `docs/design/one-id.md` names every former use and its replacement
- A4 The build-test check passes, CI's sanitizers green
- A5 x7-gate's T4/T5 rerun with fix (c) and lanes landed shows light p99 and heavy searches against master, a miss stated as a miss: one exclusive job of at most 10 minutes (D59)

## Contracts

Changes the on-disk meaning of a shard's docids and adds the bucket-to-shard table and persisted per-bucket id counters: a pre-release format change (formats open until release, D78's precedent: no migration). Automatic numeric id values keep their stride and start above every numeric id present.

## Decisions

- Owner D84 (2026-11-13): one id.
- Lead: the bucket-to-shard table is implicit (bucket % n) until the reshard quest persists one; one-id writes no table.
- D85 (lead default, owner pending): stride over a fixed B = 1024 buckets, not n, so adding or removing shards later moves buckets without renumbering ids.

## May land early

None.
