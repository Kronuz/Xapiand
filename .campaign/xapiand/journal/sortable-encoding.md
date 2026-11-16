# sortable-encoding: journal

State: working
Quest: .campaign/xapiand/quests/sortable-encoding.md (the lead's; never edit it)

## Doing

Step 1 in Xapiand: T1's `sortable_property` and T3's `sortable_xlang` as CTests over the fetched library
(pin sortable:915b014, bytes equal to 67b2a90), and the Python port adopted from `codex/py-sortable`.

## Next

1. Step 1: CTests `sortable_property`, `sortable_xlang`; port at `contrib/python/xapiand-py/xapiand/sortable.py`.
2. Step 2: the seam (versions through `Serialise::version`, sentinels, time decoders, `get_hashed` deleted), no byte change.
3. Step 3 (after the lead says the design is accepted): the switch, rename (A12), docs, T5/T6/T8 jobs.

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
