# endpoint-uuid: endpoints: UUID paths round-trip and --uuid-partition indexes open

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status endpoint-uuid`.

## Outcome

Index paths named by UUIDs round-trip, and `--uuid-partition` works: today an index path a2d6e3b4-0000-1000-... comes back as 0000000a-2d6e-63b4-...-6000 (job 261119-011017-86a323), and under `--uuid-partition` a UUID-named index cannot be opened at all, every pin throwing StorageNotFound and flush() skipping those keys (cheap-reopen review F2, probe 261119-145747-3e9285). Also `xapiand://host:port/idx` silently drops the host (endpoint.cc:210-230); refuse `:port` as malformed. Owner default: fix --uuid-partition, not remove it.

## Acceptance criteria

- A1 Find whether the UUID rewrite is Xapiand's documented normalization (compact/serialized UUID forms); either way a UUID-named index is reached by its path before and after a restart, and GET/_info name it the same way: test
- A2 Under `--uuid-partition`, creating, indexing into, searching, flushing and reopening a UUID-named index works (HTTP or node test); fails without the fix
- A3 `xapiand://host:port/idx` is refused as malformed with a clear error: test
- A4 Suite passes and CI is green

## Contracts

Index path handling (user-visible: fixes only; `:port` URIs are refused).

## Decisions

- From hunt.md rows (coverage-core-small, cheap-reopen F2); filed 2026-11-19.

## May land early

None.
