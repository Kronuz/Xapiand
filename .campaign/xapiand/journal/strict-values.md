# strict-values: journal

State: working
Quest: .campaign/xapiand/quests/strict-values.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Local build of the first cut (`build/`, light hold sv-build), then the new test_node cases and the docs E2E locally.

## Next

1. Run test_node's five new cases and the full test_node locally; fix what fails.
2. Run the docs E2E locally (harness/run_all.sh e2e); list every block that asserted the old lenient answers.
3. Commit, push for CI, `campaign verify strict-values --for A5`, A1-A4 jobs on test_node cases with `--fails-without`.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <job> |
| A2 | <job> |
| A3 | <job> |
| A4 | <job> |
| A5 | <build-test, e2e> |
| A6 | `docs/src/content/docs/changelog.md` [Unreleased] Changed, first entry |

## Result

One function, `Serialize::exact(field, type, value)` (src/serialize.cc), is the rule every typed value meets:
- field writes: `Schema::index_item` (integer, positive, floating fields);
- `_id` writes: the `_id` branch of the index path in src/database/schema.cc and `Serialize::id`;
- searches: `QueryDSL::get_regular_query` (terms, numeric `_id` included), range bounds (`exact_range` for object
  ranges, `parse_range` for text ranges and `q=` ranges), `get_acc_num_query`; `numeric_bound` in
  src/multivalue/range.cc now takes only numbers, which QueryDSL hands it.
The SerializationError fallbacks to a global query in `get_value_query` and `get_in_query` are gone (lead ruling).
`Cast::integer` and `Cast::positive` refuse values outside their range instead of wrapping (A3).
Docs: "Exact Values" in reference-guide/schemas/field-types/numeric-type.md (E2E blocks), casting-values.md and
index-api.md point to it.

## Rulings I made

- R1 "Exactly a value of the type" is by value, not by JSON spelling: an integral float (12.0, 4294967297.0) is that
  integer, and text reads as JSON reads a number (int64, uint64, then double), so "12.0" is 12. 1.0 in an integer field
  was refused on writes before; it is taken now, the same as in searches and ids.
- R2 The rule checks integer, positive, floating, keyword, text and uuid; other types (boolean, dates, times, geo)
  keep their own readers, which already refuse a value they cannot read; with the fallback gone those refusals are
  400s too, with their own messages.
- R3 A floating field keeps taking ±inf (tests/numeric_range_check.py's `inf` group writes them); NaN stays refused.
- R4 Range bounds of a field with no type (the global path) are read as floats would be (any number, numeric text):
  there is no type to hold them to.
- R5 Sort-by-distance values (`_sort` with `_value`) keep casting: they order, they do not select.
