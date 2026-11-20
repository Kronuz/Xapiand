# dsl-date-operators: journal

State: waiting on jobs 261119-225927-d1b679 (probe) and its mutant
Quest: .campaign/xapiand/quests/dsl-date-operators.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Implemented: _date/_time/_timedelta join the field/global cast-word dispatch in src/query_dsl.cc (_date is cast as _datetime, as Serialize::object has no DATE case; global _time/_timedelta name their type, as guess_serialize reads them as datetimes). HTTP test tests/test_query_dsl_date_http.py boots a node. Job 261119-222203-a28c39 (build + test) is the first probe; expected values for global queries unverified.

## Next

1. Read the job output; fix expectations or code. 2. Mutant job (--fails-without: run the test at master query_dsl.cc). 3. `campaign verify dsl-date-operators` for the suite (A3); push is the lead's.

## Blocked on

Docs (outside my file set): casting-values.md:105-135 and its e2e assert `_date` answers 400 "Invalid operator"; it fails once this lands. The lead should assign the docs edit.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- The sorter's cast-word list (get_sorter) is unchanged: sort cast words are a separate bug (sort-cast-word) and the sorter has no time/timedelta key.
- No edit to serialize.cc (outside the file set): the DATE and global TIME gaps are bridged in query_dsl.cc.

- Lead widened the file set to casting-values.md: its edit (lines 105-135 and its e2e block, in the same file) waits for the probe job output, so the examples carry checked replies.

- Probe 222203 failed 8: ids compared as strings but _id is an int (test bug, 5 field checks; the code answered right); a text _date inside _datetime is a clear 400 (expectation fixed); a float into a datetime field is a timestamp, 200 [] (expectation fixed); global queries matched nothing because no value was indexed globally (fixture: /gl/ index with _index global, plus a _datetime control). casting-values.md waits for the 225927 output.
