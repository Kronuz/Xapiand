# faultlab-adopt-xapiand: Xapiand harness/cluster uses faultlab history and proxy; its copies go (migration step 4)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-adopt-xapiand`.

## Outcome

Migration step 4 of cpp-libraries `docs/design/faultlab.md`: Xapiand's harness/cluster imports faultlab's history checker and proxy tier (pinned by commit), and its own history.py, proxy.py, proxy_test.py and selftest.py go. No shim.

## Acceptance criteria

- A1 The cluster harness's checks behave as on master with faultlab's copies: one job runs faultlab's self-test, `run.sh check --nodes 1` (every item passes) and `run.sh check --nodes 3 --expect-fail membership,read,search,aggregations,failover,replay` (multi-node membership is master's known gap until cluster-gate; the run passes only if exactly those items fail), building xapiand at the job's own commit
- A2 `git grep -l 'class Link\|def check_linearizable' harness/` finds nothing in Xapiand: a job

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
