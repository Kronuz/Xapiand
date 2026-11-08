# prometheus-overflow: Instrument fetched libraries with sanitizers

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:55316114d (2026-10-26); its ledger `docs/status/prometheus-overflow.md` is the record. Its machine state is in `state.json`: `campaign status prometheus-overflow`.

## Outcome

Sanitizer flags are configured before FetchContent dependencies compile, instrumenting prometheus-cpp and other fetched libraries. The reported Summary overflow is attributed to the prior uninstrumented dependency build, not a confirmed race.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Move `find_package(Sanitizers)` before `include(FetchContent)` so fetched source targets receive sanitizer flags: `docs/status/prometheus-overflow.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The ASan container-overflow is classified as a false positive caused by an uninstrumented dependency.
- Risk low: Build configuration and sanitizer instrumentation only (the lead's mapping).

## May land early

None.
