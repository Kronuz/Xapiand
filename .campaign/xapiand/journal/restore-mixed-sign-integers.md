# restore-mixed-sign-integers: journal

State: waiting on jobs 261111-171323-80d18b 261111-171345-4024c7 261111-171348-d191ec
Quest: .campaign/xapiand/quests/restore-mixed-sign-integers.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix and test committed; jobs for A1 (head pass, master-src fail) and A2 (corpus round trip) submitted.

## Next

1. Read the jobs; write docs/dev/restore-integers.md (A3) with their numbers.

## Cause (found 11-11)

`Schema::guess_concrete_type` typed a field by its first integer's msgpack *encoding*: POSITIVE_INTEGER ->
`positive`, NEGATIVE_INTEGER -> `integer`. msgpack packs every non-negative integer unsigned, so a dump whose
`big` starts positive types it `positive` (concrete), and each later negative value reaches
`Schema::index_item`'s positive branch, `value.u64()` (src/database/schema.cc), whose msgpack `type_error`
is the 500. JSON input mostly escaped: the rapidjson adaptor (msgpack xchange/rapidjson.hpp) makes values
that fit int32 or int64 NEGATIVE_INTEGER (signed), but [2^31, 2^32) and >= 2^63 POSITIVE_INTEGER, so JSON hits
the same 500 for a field whose first value is in [2^31, 2^32). Strings already used a value rule
(`Serialise::guess_type`: fits int64 -> integer, else positive). Not a msgpack library bug: `u64()` of a
negative value is right to throw; the caller is wrong.

## Blocked on

Owner confirmation (not blocking the review): one value rule means a field whose first msgpack integer is
non-negative now infers `integer` (was `positive`), so a later value >= 2^63 in it is refused with 400 where it
was stored before. JSON input already behaved so. Options: 1. accept (recommended; one rule for JSON, msgpack
and strings, as Elasticsearch infers `long`; full uint64 range stays available by declaring `positive`);
2. keep encoding-based inference and widen a `positive` field to `integer` on its first negative value (needs a
schema type migration of terms already written; not attempted).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261111-171323-80d18b; fails without: 261111-171345-4024c7 |
| A2 | 261111-171323-80d18b (3000 of 3000 round-trip, compared one by one) |
| A3 | `docs/dev/restore-integers.md` |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- One integer typing rule, by value: `Serialise::guess_type` (and `guess_serialise`, used for `_id`) gives
  `integer` for any integer that fits int64, `positive` beyond; `guess_concrete_type` uses it for both msgpack
  integer types, and accepts a value into an existing integer/positive type when it fits its range.
- A value outside an integer field's range (a negative into `positive`, >= 2^63 into `integer`, a float into
  either) answers 400 "Format invalid for ... type" instead of msgpack's 500 `type_error`.
- A1's test is `tests/restore_cli_check.sh` (its generated documents gain `n` and `w`, positive first then
  negative, `w` beyond 2^31), not a new tests/test_restore_integers.cc: registering a ctest needs CMakeLists.txt,
  outside the file set, and the script restores through the real binary (HTTP msgpack RESTORE and CLI --restore).
  The script also checks each RESTORE's `indexed` is N (it answered 200 with 1498 of 3000 before), and takes
  `CORPUS=<ndjson>` for A2.
