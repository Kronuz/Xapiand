# serialize-repin: Repin the renamed libraries and finish serialize in Xapiand

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status serialize-repin`.

## Outcome

Xapiand builds against the libraries renamed to American spelling (owner, 2026-11-16: "the sooner
we rename all serialise to serialize, the better"), with no British `serialis*` name left in its
own code, tests, harness or docs, and no alias. Behavior and bytes are unchanged.

## Acceptance criteria

- A1 Pins: CMakeLists.txt pins varint:4dd60a8, hashes:423c8cc, datetime:b5d46a8, cuuid:dad68f8, msgpack:1e9aaaf, string-similarity:a5f50a6, flume:19e10d9 and prospect:eef601d (full shas, or later main commits), together with the reactor thread-name repin and system:563880a (hunt rows); the full suite passes: the `build-test` check
- A2 No British spelling: `git grep -n -i -I serialis -- . ':!.campaign'` prints only Xapian proper names in provenance notes (`sortable_serialise`, `serialise-double.h`), each listed in the journal; the `SerialisationError` alias in src/exception_engine.h is gone: a job running that grep
- A3 Behavior unchanged: the docs E2E passes: the `e2e` check
- A4 Stale names the sortable-encoding rename left (harness/coverage.sh, TESTING.md, the exact_id_mutants query-old-codec mutant) are renamed and still run: a job running each

## Contracts

Consumes the renamed APIs of the libraries in A1. Provides nothing new.

## Decisions

- Owner, 11-16: rename everything now, mechanically, with no aliases (migrate callers, delete old names).
- Lead, 11-16: a separate quest after sortable-encoding, so that quest's accepted design review stands; cluster and server are archived and keep their old spelling.

## May land early

None.
