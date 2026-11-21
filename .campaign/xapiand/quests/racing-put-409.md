# racing-put-409: A replace that lost a delete-and-recreate race is a version conflict, not a 500

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
is in `state.json`: `campaign status racing-put-409`.

## Outcome

When a replace by id reaches the strata shard and the key sits at another docid, the handler's id
lookup lost a race with a delete-and-recreate. The shard (src/engine/strata/shard.cc) now throws
`version_conflict` instead of `errc::invalid`, so an unversioned write is retried and a versioned
one answers 409, like any other lost race; nothing is applied. Before, it answered 500 "The key
belongs to another document". From the hunt list's racing-PUT row.

## Acceptance criteria

- A1 A replace by a stale docid is a version conflict: tests/test_engine_strata.cc and
  tests/test_node.cc, failing at master (explorer job 261121-095112-fac804) and passing at the fix
  (261121-095122-f2ce08), rerun at the landed code
- A2 The suite passes at the landed code: the `suite` check

## Contracts

None: a 500 under this race becomes a retry or a 409.

## Decisions

- Lead, 2026-11-21: lands ahead of journal-replaces-wal, which holds these files and rewrites the
  write path; the five lines merge into it, and its contributor keeps the conflict through the
  prepare/apply split.

## May land early

None.

## Tasks

None: explore/xapiand/racing-put-409 (Xapiand:cc8c27362, Xapiand:01ec326b6).
