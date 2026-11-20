# strata-pin-catalog: Pin strata at the catalog delivery fix; land test_pool's reproducer

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-pin-catalog`.

## Outcome

Xapiand pins strata at strata:e3edb426 (catalog-post-lifetime: a delivery never posts to a torn-down scheduler), and tests/test_pool.cc carries the explorer's reproducer `test_a_delivery_never_posts_to_a_torn_down_scheduler` from branch explore/pool-uaf (Xapiand:7b89dc9f3), which failed 5/5 under ASan at the old pin (job 261119-210411-ddceb2).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 test_pool passes under ASan+UBSan at the new pin, the reproducer included, five runs: a job
- A2 The reproducer fails at the old pin (397c02d5): a fails-without job
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
