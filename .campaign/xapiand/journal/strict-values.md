# strict-values: journal

State: waiting on jobs 261116-092507-997086 261116-092507-845129 261116-092510-5a755e 261116-092513-8e8db4 261116-092516-7d49d2 261116-092519-a7c977 261116-092523-55e91e
Quest: .campaign/xapiand/quests/strict-values.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the gate checks and the criterion jobs at 710beb9f3.

## Next

1. When the jobs finish: paste `campaign brief`'s evidence table below, set State to "ready for review".

## Blocked on

Nothing. Outside my file set, for the lead:
- src/database/handler.cc:1639 (`get_prefixed_term_id`, GET/PUT/DELETE by a path id) still runs `Cast::cast` before
  `Serialize::id`: a path id is text, so it is already strict ("abc", "-1" on positive: 400), but its message is
  Cast's ("Value "-1" cannot be cast to positive", no field), and "12.0" is a 400 there while a body id 12.0 is 12.
  Fix: `Serialize::id(spc_id, document_id)` (it applies the rule). One line in the other quest's file.
- `PUT /idx/1.5` writes id 1: in a URL path a dot starts a selector (URL syntax, src/server), so a fractional id can
  only come in a body (RESTORE); the tests and the docs example use that.
- tests/numeric_range_check.py (manual T6 check, not in ctest) group `intbound` asserts float bounds on integer and
  positive fields (design D6), which D92 now refuses with 400; its `text` group's fractional text bounds on the integer
  index likewise. Needs updating with D92.

## Evidence

Local, at 710beb9f3: ctest (41 other suites) 100%, test_node full (3205 checks before the last case was added, then the five D92 cases), docs E2E
GREEN (496 requests, 770 assertions; it failed once on /_all/ object-field searches, fixed by 710beb9f3).

| Criterion | Evidence |
|---|---|
| A1 | 261116-092510-5a755e (cases at head); 261116-092513-8e8db4 (--fails-without: fails on master's src) |
| A2 | 261116-092510-5a755e; 261116-092516-7d49d2 (--fails-without) |
| A3 | 261116-092510-5a755e; 261116-092519-a7c977 (--fails-without) |
| A4 | 261116-092510-5a755e; 261116-092523-55e91e (--fails-without) |
| A5 | build-test 261116-092507-997086, e2e 261116-092507-845129 |
| A6 | `docs/src/content/docs/changelog.md` [Unreleased] Changed, first entry |

Existing tests and E2E blocks changed: none. Prose changed: changelog's sortable entry no longer lists "float bounds on
integer fields" (D92 refuses them); casting-values.md and index-api.md point to the new Exact Values section.

## Result

One function, `Serialize::exact(field, type, value)` (src/serialize.cc), is the rule every typed value meets:
- field writes: `Schema::index_item` (integer, positive, floating fields);
- `_id` writes: the `_id` branch of the index path in src/database/schema.cc and `Serialize::id`;
- searches: `QueryDSL::get_regular_query` (terms, numeric `_id` included), range bounds (`exact_range` for object
  ranges, `parse_range` for text ranges and `q=` ranges), `get_acc_num_query`; `numeric_bound` in
  src/multivalue/range.cc now takes only numbers, which QueryDSL hands it.
The SerializationError fallbacks to a global query in `get_value_query` and `get_in_query` are gone (lead ruling);
a field with no value type (no type yet, object, array, foreign, script) is still searched globally.
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
- R6 A body `_id` given as a cast object is not a cast: the write path reads a map `_id` as an object (pre-existing,
  "Object already has a value"); casts on `_id` work in searches.
- R5 Sort-by-distance values (`_sort` with `_value`) keep casting: they order, they do not select.
