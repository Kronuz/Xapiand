# sortable-encoding: journal

State: waiting on jobs 261115-224734-3b43cf 261115-224734-8ff56e 261115-225548-6cc0f3
Quest: .campaign/xapiand/quests/sortable-encoding.md (the lead's; never edit it)

## Doing

Steps 1 and 2 done (no byte change); their checks are running. `tests/numeric_range_check.py` (T6, A5)
written (Xapiand:6e8bd448f). A debug run on a pre-switch binary (one-id-rel slot, Xapiand:1f5b791d2) shows
the old failures it targets: range 189 wrong of 788, sort 6 of 8, intbound 4 of 11, wide 1 of 7 (a 6.5
bound truncated on the integer-typed index), NaN bound and NaN value accepted. Baseline job
261115-225548-6cc0f3 records it on the step-2 build (exit 1 expected).

1. Step 1, Xapiand:49deb7184: CTests `sortable_property` (T1, from `${sortable_SOURCE_DIR}/test/property.cc`)
   and `sortable_xlang` (T3: `tests/sortable_xlang.py` against `sortable_cli` from `tools/cli.cc`); the port
   merged from `codex/py-sortable` and moved to `contrib/python/xapiand-py/xapiand/sortable.py`; pin
   sortable:915b014. Mutants in `tests/sortable_xlang_mutants.sh`.
2. Step 2, Xapiand:8a7da1f4a: versions through `Serialise::version`/`Unserialise::version` (engine adapter
   too), time decoders through `Unserialise::floating`, `get_hashed` deleted. The old codec is called only
   from `src/serialise.h` (and the GPL files themselves).

## Next

1. When build-test 261115-224734-3b43cf and e2e 261115-224734-8ff56e finish: fix anything they show.
2. Step 3 (after the lead says the design is accepted): the switch in `src/serialise.h` (trusted decode for
   stored bytes, strict for user bytes), D4 400s, D6 strict_stod and ±inf open bounds, `git rm` the GPL
   files and module, rename (A12), docs (A10, A11), T5/T6/T8 jobs.

## For the lead

- Design revised to D87 at Xapiand:3f95a5edd ("Revise the sortable-encoding design for D87"): D8 is
  now "no marker, no compatibility", T8 (marker) is gone and T8 is now speed (A7). F1, F8, F9, F10 are
  obsolete with the marker. F2 (blocking) is answered: T1 and T3 are CTests over the fetched library's
  fixtures with sha256 asserts, so CI runs them on every platform. F3-F7, F11, F12 are folded into D6,
  T1, T3, T5, T6, T7. Please have the design reviewer confirm.
- File set, needed before step 3:
  1. A12's grep covers `docs` and `harness` whole (about 60 docs files, including docs/reviews,
     docs/perf raw JSON, and harness/async_check.py, harness/test_gate_tools.py). The file set has only
     five docs paths and no harness. Add `docs/**` and `harness/**`, or narrow A12.
  2. A8 says "the check added to `harness/seam_check.sh`"; that file does not exist. The quest's
     `no-gpl` check already runs the grep; I propose A8 rest on that check.

## Blocked on

Step 3 on the design reviewer's confirmation of the D87 revision. Steps 1 and 2 are not blocked.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | D87 (PLAN.md decision row, owner 2026-11-13) |
| A3 | 261115-224157-3d7604 (pass at 49deb7184: 113,491 vectors, 10^6 random, 1,541,467 decodes; brief says "ran other code" after later commits, rerun at the final head); mutants killed: window-62 261115-224339-653d9d, rule-7 261115-224809-f01fd9 (`decode 0200: python i -2^63, C++ reject`). rule-8 261115-224330-733fc7 survived: equivalent (rule 8 is implied by rule 9, as sortable's note says) |

Jobs at step 2 (8a7da1f4a): build-test 261115-224734-3b43cf, e2e 261115-224734-8ff56e (running);
no-gpl 261115-224735-2ae37e failed as expected (the GPL files go in step 3).

## Result

<When ready for review.>

## Rulings I made

- Pin sortable:915b014 (newest main): bytes and fixtures equal sortable:67b2a90 (sha256 of
  vectors.txt c4d4a425..., reject.txt a679d215..., model.py b8c29680... at 67b2a90, ac13b77, 915b014);
  it adds the trusted typed conversions D87's speed gate needs.
- D88 spelling: `Serialise` becomes `Serialize`, `Unserialise` `Unserialize`, `SerialisationError`
  `SerializationError`, `serialise*.{cc,h}` `serialize*` (the direct respelling).
- Python module path `xapiand/sortable.py` (not the GPL file's `xapiand/utils/sortable.py`, so the
  diff never shows the GPL text being replaced in place).
- An absent range bound encodes as -inf/+inf for every numeric type (design D6, review F4c).
