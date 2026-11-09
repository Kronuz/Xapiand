# alpha10-builds: journal

State: landed
Quest: .campaign/xapiand/quests/alpha10-builds.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:dee2deaad on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/alpha10-builds.md`: "xapian.h include path for XAPIAN_OBJ; compressors pin 816bc82" |
| A2 | `docs/status/alpha10-builds.md`: "FindSanitizers: `-fno-sanitize=function` only for Clang." |
| A3 | `docs/status/alpha10-builds.md`: "WAL task members initialised; AggregationMatchSpy::unserialise argument order; test_pool local node" |
| A4 | `docs/status/alpha10-builds.md`: "test_engine_sort build and the two random-bytes tests skipped when asserts are on" |
| A5 | `docs/status/alpha10-builds.md`: "XAPIAN_ASSERTS option (the vendored Xapian's assertions; on in the CI assert job and in Debug)." |
| A6 | `docs/status/alpha10-builds.md`: "tls_check: s_client for the untrusted-CA refusal" |

## Result

Landed at Xapiand:dee2deaad on 2026-10-28. Docker memory raised from 2 to 8 GiB for repro builds, then restored to 2 GiB; container and volume removed. No review named in the ledger.

- Merge: Merge branch 'xapiand' (alpha.10 build fixes, copyright).
- Compressor pin: 816bc82; clash fix `compressors:9d2a87c`.
- CI was green on every job at the last push before the final commit.
- Release, FreeBSD, and Bottles publishing workflows were not run on the branch.
- Open: lead merge to `xapiand`, then master.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
