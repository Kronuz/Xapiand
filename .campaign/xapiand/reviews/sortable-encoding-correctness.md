# sortable-encoding review: correctness

Commit: Xapiand:4d31b411d
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- no-gpl 261116-041115-77cd6d (A8) and the A12 grep 261116-041119-07b968: no `sortable_serialise` left in
  src or contrib, so every reader and writer goes through `Serialize::`/`Unserialize::` (src/serialize.h:142-194,
  :300-350). I did not re-grep.
- T3 identity and its killed mutants (261115-224339-653d9d, 261115-224809-f01fd9): C++ and Python bytes agree.
- Datetime floor fix Xapiand:2c26c80e9: fails-without job 261116-015759-0b2751 reverts the two `std::floor`
  calls and `test_datetime_range_finds_every_value_in_range` fails; the value side builds its terms from a
  `tm_t` (1969 for -0.5 s), so floor is the matching rule (src/multivalue/generate_terms.cc:580-582).
- T6 (`tests/numeric_range_check.py`) compares every answer with Python's exact int/float comparison
  (brute force) on 2^k +- d, -0.0, NaN, +-inf, ids above 2^53, the midpoint text and mixed-type wide ranges;
  old-build fails-without 261115-232755-9ae906. Head runs (36d20e, 3d3040, build-test, CI 43864388357) were
  still running when I wrote this; I did not wait on them.

## What I checked myself

- Normalized diff: the merge-base tree respelled `serialis` -> `serializ`, diffed against 4d31b411d
  (GPL files excluded unread). Everything outside serialize.{cc,h}, range.cc, schema.cc's floating case,
  generate_terms, cast, keymaker.h, the version slot sites and the new tests is a pure respelling.
  contrib/javascript/cuuid and contrib/python/cuuid differ only in spelling (exported names and messages).
- Order and exactness by reading: one codec for int64, uint64, double, timestamps and the version slot;
  a floating field stores an integer's exact bytes (schema.cc:5358-5373, serialize.cc:423-447); range bounds
  in their own MsgPack type (range.cc:83-114); truncation for accuracy terms is monotone on both sides, so
  term covers stay supersets; an absent bound is -inf/+inf; `MAX_CMPVALUE` is `encode(DBL_MAX)` as before.
- Probe P1: `.scratch/review-sortable-encoding-correctness/probe_text_bounds.py BIN [PORT]`, run under
  `quiet light` against a copy of the contributor's build-local binary (built 01:43 from the head's code
  before the rename commit, sortable 915b014, byte-equal to the 2fad5a2 pin per the journal). Output:
  "4 wrong" (below). Rerun at the fix with the build-test binary: `python3 <that path> build/bin/xapiand`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| P1 | yes | defect | A number written as text in a range bound on a floating field parses as a double (`Cast::floating` -> `strict_stod`), while the field now stores integers exactly. On a floating field holding a=2^53, b=2^53+1, c=2^53+3 (JSON integers), d=2^53+4: the string range `["9007199254740993","9007199254740993"]` answers [a], want [b]; `["9007199254740993","9007199254740995"]` and the query string `f:9007199254740993..9007199254740995` answer [a,b,c,d], want [b,c]; the same string range over `/p*/` (an integer and a floating index) answers [a,a], want [b,b]. The term forms agree with the stored bytes (`{"f": "9007199254740993"}` and `q=f:9007199254740993` answer [b]), so `f:X` and `f:X..X` disagree on the same text. Before the switch both sides were doubles (a superset including b); now b is dropped and a returned. | src/multivalue/range.cc:108-113 (`default: return floating(Cast::floating(value))`), src/cast.cc:215-222; probe P1 output | In `numeric_bound`, a STR bound parses exactly: integer text as int64/uint64 bytes (strict_stoll / strict_stoull), other text as strict_stod, whatever the field type (so the wide case also holds). Test: T6 gains a group with odd integers above 2^53 on a floating field, queried by string bounds in the DSL and by `q=f:a..b`, plus the `/t6w*/` wide range with string bounds; it must fail at 4d31b411d. The owner may instead rule text bounds on floating fields are doubles, but then the term path must match it, and `f:9007199254740993` would miss the document whose value is that number. |
| N1 | no | nit | The A12 rename changes one wire name: the sort keymaker registry string `"SerialiseKey"` became `"SerializeKey"` (sent in serialized sorters between nodes). Both ends of one build agree; a mixed-version cluster would not, which D87's dump-and-restore already excludes. No stored key or document field changed spelling. | src/multivalue/keymaker.cc:628,660; keymaker.h:102 | Mention in the changelog's upgrade note, or leave. |
| N2 | no | nit | `MAX_CMPVALUE`'s comment says a missing value sorts "after every finite number"; it is `encode(DBL_MAX)`, so it ties with a stored DBL_MAX (same as before the switch). | src/multivalue/keymaker.h:47-50 | Say "at DBL_MAX". |

P1 output (abridged):

```
WRONG pf string range ['9007199254740993','9007199254740993']: got ['a'] want ['b']
WRONG pf string range ['9007199254740993','9007199254740995']: got ['a', 'b', 'c', 'd'] want ['b', 'c']
WRONG pf query string f:9007199254740993..9007199254740995: got ['a', 'b', 'c', 'd'] want ['b', 'c']
WRONG p* string range [2^53+1, 2^53+1] (pn b + pf b): got ['a', 'a'] want ['b', 'b']
ok    pf JSON term / JSON range / string term / q=f:9007199254740993 / sort asc
4 wrong
```

## Not run

- No build of my own: P1 ran against the contributor's 01:43 build-local binary, not a job binary at
  4d31b411d; the code paths it exercises (range.cc, cast.cc, serialize.cc) are unchanged since except for
  spelling.
- Head results of build-test, e2e, T3, T5, T6, A12 and CI 43864388357 were pending; A6/A7 speed and size
  are the lead's measurement jobs and outside this lens.
- Aggregation buckets (histogram/range) and keymaker distance sorts read numbers as doubles; I did not
  probe their precision above 2^53 (not claimed exact by the quest).
- Lua/script bindings: script.cc changes only spelling; I did not trace whether scripts ever see slot bytes.
- docs/** spelling (A12 waits for the file set) and the JS package's renamed exports' consumers.
