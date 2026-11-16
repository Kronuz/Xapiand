# write-path-fixes: Document id 0 and the ?version check on writes

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status write-path-fixes`.

## Outcome

Two write-path defects found by the docs-site audit's probes (explorer job 261109-183815-526fac at
master 61ddc922d) are fixed. A document with id 0 is stored as id 0: today `PUT /z/0` answers 204,
`GET /z/0` answers 404, and a following `PUT /z/1` returns `_version: 2`, so the id-0 write landed on
id 1 (another document overwritten). And a write that names `?version=N` succeeds only when the
document's current version is N, else 409 Conflict and nothing changes; today DELETE and PUT ignore
it (`src/server/search_views.cc:833-851`, `src/database/shard.cc:1448-1462` take the version
`[[maybe_unused]]`), so `DELETE /tv/1?version=1` deletes version 2. Risk high: it decides whether
user writes land on the right document.

## Acceptance criteria

- A1 The cause of the id-0 write is named in the journal (where 0 is read as "no id" or as another id) and fixed there; ids 0, 1, -1, "0", "00" and the largest integer id round trip through PUT, GET, SEARCH and DELETE without touching any other document: a test that fails at master's code, run by `build-test`
- A2 `?version=N` on PUT, PATCH, UPSERT, MERGE and DELETE: the current version matches → applied and the version advances; mismatch → 409 with the current version in the body and the document unchanged; absent document with a version → 409 (or 404 for DELETE, the journal says which and why, matching Elasticsearch's `if_seq_no` behaviour where it is clear): a test that fails at master's code
- A3 Writes without `?version` behave exactly as before (no regression): the existing suites in `build-test`, and the docs E2E (`harness/run_all.sh smoke e2e tls`) as a job
- A4 Concurrent writers with the same expected version: exactly one succeeds and the rest get 409, under TSan: a job (the race between the read of the version and the write is the review's question)
- A5 The `build-test` check passes, and CI is green

## Contracts

The HTTP API's `version` parameter becomes enforced as documented; no new parameter.

## Decisions

- Lead, 11-09: both are fixes, not narrowing. The docs pages get E2E tests for id 0 and for version conflicts in docs-site-refresh (D47).

## May land early

Nothing; one landing.
