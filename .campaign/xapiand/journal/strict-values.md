# strict-values: journal

State: waiting on jobs 261116-102318-c7cf29 261116-102322-382d53 261116-102327-00286b 261116-102331-08b7c8 261116-102336-f2cb97 261116-102340-55981a 261116-102345-c19625 261116-102352-9b6620 261116-102352-d620da 261116-102358-db013f 261116-102359-52fc3b 261116-102359-659f2d
Quest: .campaign/xapiand/quests/strict-values.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Review strict-values-correctness (CHANGES at 710beb9f3): blockers B1 (C1), B2 (C2), B3 (C3), and the N-row (C4).
- B2: `exact_number` refuses an integral float with |d| >= 2^53 for integer and positive (src/serialize.cc); case
  `test_a_float_from_two_to_the_53_is_no_exact_integer`.
- B3: `Cast::bare_number_text` (src/cast.cc): numeric text with white space at either end is no number, in `number_of`
  and in `Cast::integer/positive/floating`; case `test_numeric_text_with_white_space_is_no_number`.
- B1: `.scratch/strict-values/fails_on_src.sh BASE CASE...` (home checkout; harness/ is outside the file set) puts src/
  back to BASE, drops only the test_node functions that use Serialize::exact when BASE lacks it, builds, prints each
  case's failed checks, and exits 0 only when the build succeeds and every case fails. Replaces cases_on_master_src.sh.

## Next

1. When the jobs finish: read each `--fails-without` log (every case must fail on its checks, not a build error), paste
   `campaign brief`'s evidence table below, set State to "ready for review".

## Review fixes (strict-values-correctness, CHANGES at 710beb9f3; fixed at Xapiand:8440e071e)

Local at 8440e071e: the seven D92 cases pass (test_node, incremental build under quiet light).
- B1 (C1)=JOB 261116-102322-382d53 (A1), 261116-102327-00286b (A2), 261116-102331-08b7c8 (A3), 261116-102336-f2cb97 (A4):
  `--fails-without`, `fails_on_src.sh 26ab9c5a1` (master's src); reviewer's honest cut rerun: 261116-102359-52fc3b.
- B2 (C2)=JOB 261116-102340-55981a (`--fails-without`, src at 710beb9f3); probe rerun 261116-102358-db013f.
- B3 (C3)=JOB 261116-102345-c19625 (`--fails-without`, src at 710beb9f3); probe rerun 261116-102359-659f2d.
- Cases at head: 261116-102318-c7cf29. Gates: build-test 261116-102352-9b6620, e2e 261116-102352-d620da.
- C4 (N-row): not implemented, see Blocked on. C5, C6, C7 (nits): unchanged.
- Stale jobs I could not cancel (cancel was denied): build-test/e2e at 710beb9f3 (997086, 845129) and at b31cb56e6
  (e934f3), and the invalid 261116-092523-55e91e; the A1-A4 rows above replace 8e8db4, 7d49d2, a7c977, 55e91e.

## Blocked on

- Lead (C4, N-row; the lead's default, the owner may rule otherwise): a search over indexes that type a field
  differently should check the value per index (a fitting index searches it, a non-fitting one gives no hits; 400 only
  when it fits none). Not done: the search builds one query from one schema, `endpoints[0]`'s (SchemasLRU::_update,
  src/database/schemas_lru.cc:322; `QueryDSL query_object(schema)` in DatabaseHandler::get_mset,
  src/database/handler.cc:1262). Per-index typing needs every endpoint's schema there and a query per distinct schema
  (OR'd, or one per shard), in handler.cc and schemas_lru.cc, outside this quest's file set and well over 60 lines.
  Until then such a search answers 400 when the value doesn't fit the first index's type (master: 200 through the
  global query). Options: (1) keep that and document it; (2) a follow-up quest owning handler.cc/schemas_lru.cc for
  per-index queries (recommended); (3) fall back globally only with several endpoints (narrow, but untyped).

Outside my file set, for the lead:
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
| A1 | 261116-102318-c7cf29; fails without: 261116-102322-382d53, 261116-102340-55981a, 261116-102345-c19625 |
| A2 | 261116-102318-c7cf29; fails without: 261116-102327-00286b, 261116-102340-55981a |
| A3 | 261116-102318-c7cf29; fails without: 261116-102331-08b7c8, 261116-102345-c19625 |
| A4 | 261116-102318-c7cf29; fails without: 261116-102336-f2cb97 |
| A5 | build-test 261116-102352-9b6620, e2e 261116-102352-d620da |
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
- R7 (B2) An integral float of magnitude 2^53 or more is no integer or positive value (from 2^53 doubles are 2
  apart, so a fraction can round to one); below, R1 holds. Text without '.' or exponent still reads exactly through
  int64/uint64. Note: a fraction finer than the double's precision still rounds to an integer below 2^53 (e.g.
  9007199254740990.5 is the double 9007199254740990.0, since [2^52, 2^53) has spacing 1); only the JSON text could
  tell, and for a JSON number the parser has already read the double.
- R8 (B3) Strict, not trim: numeric text with white space at either end is no number, in `Serialize::exact` and in
  every Cast of text to integer, positive or float (one check, `Cast::bare_number_text`). C5's "+12" and "0x10"
  are not changed (non-blocking nit).
- R5 Sort-by-distance values (`_sort` with `_value`) keep casting: they order, they do not select.
