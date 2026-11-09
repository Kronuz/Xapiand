# manifest-log-prospect: prospect moves to strata's manifest log

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status manifest-log-prospect`.

## Outcome

prospect pins strata at the head of `manifest-log` (later its landed main commit) and its tests read and damage the manifest log instead of the retired files (`files::manifest`, live-docs, metadata and `fields-*` files, `meta-1.kv`); prospect's behavior is unchanged (Kronuz/prospect; manifest-log journal, Blocked on 2 and the lead's L1).

## Acceptance criteria

- A1 `format13_test.cc`, `reclaim_test.cc`, `media_test.cc`, `fixtures_test.cc` and `directory_test.cc` exercise the same cases on the manifest log (no case dropped; each damage case still damages what it names): the `build-test` check
- A2 prospect pins strata at the commit where `manifest-log` lands: the `build-test` check at the landed `CMakeLists.txt`
- A3 prospect's suite passes and its churn and dump output is identical to prospect on strata's main before `manifest-log` at flush threads 1 and 2: the `build-test` check and a churn job run in prospect's repository (`--ref strata=../strata@... --ref main=../strata@...`, strata's `test/prospect_integration.sh`)

## Contracts

Consumes `strata-manifest-log` (strata `docs/design/manifest-log.md`).

## Decisions

- Built against the `manifest-log` branch head while it is open; repinned to strata's main commit before landing (`--after manifest-log`, landing ordered, never atomic).
- manifest-log's A9 jobs run prospect at this quest's branch.

## May land early

None.
