# faultlab-adopt-xapiand: Xapiand harness/cluster uses faultlab history and proxy; its copies go (migration step 4)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-adopt-xapiand`.

## Outcome

Migration step 4 of cpp-libraries `docs/design/faultlab.md`: Xapiand's harness/cluster imports faultlab's history checker and proxy tier (pinned by commit), and its own history.py, proxy.py, proxy_test.py and selftest.py go. No shim.

## Acceptance criteria

- A1 The cluster harness's checks pass on faultlab's copies: the harness self-test and one cluster run as jobs
- A2 `git grep -l 'class Link\|def check_linearizable' harness/` finds nothing in Xapiand: a job

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
