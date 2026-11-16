# alpha10-builds: Stabilize alpha10 build and CI

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:dee2deaad (2026-10-28); its ledger `docs/status/alpha10-builds.md` is the record. Its machine state is in `state.json`: `campaign status alpha10-builds`.

## Outcome

CI and local builds accommodate Xapian assertions, Clang sanitizer flags, and runner TLS behavior. The alpha10 container and volume were removed, and Docker memory was restored to 2 GiB.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Fix XAPIAN_OBJ include path and pin compressors at 816bc82: `docs/status/alpha10-builds.md`
- A2 [artifact] Apply `-fno-sanitize=function` only for Clang: `docs/status/alpha10-builds.md`
- A3 [artifact] Initialize WAL task members; fix aggregation argument order and test-pool setup: `docs/status/alpha10-builds.md`
- A4 [artifact] Build `test_engine_sort`; skip two random-bytes tests when assertions are enabled: `docs/status/alpha10-builds.md`
- A5 [artifact] Add `XAPIAN_ASSERTS`; skip known Xapian public-API assertion probes in assertion builds: `docs/status/alpha10-builds.md`
- A6 [artifact] Poll `tls_check` for up to 6 s and use `s_client` for untrusted-CA refusal: `docs/status/alpha10-builds.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Keep Xapian's public-API assertion bug out of assertion-enabled test probes.
- Restrict the function sanitizer exclusion to Clang.
- Risk low: Build, test, and CI tooling changes only (the lead's mapping).

## May land early

None.
