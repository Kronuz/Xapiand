# restore-stream: journal

State: working
Quest: .campaign/xapiand/quests/restore-stream.md (the lead's; never edit it)

## Doing

Capturing master's answers: commit A (tests, note, probe; no src change) runs
`tests/test_restore_stream.cc` with RESTORE_STREAM_CAPTURE=1; commit B is the change.

## Next

1. Paste master's captured answers into `expected_answers()`; the A4/refused rows from B's capture.
2. Jobs: suite (A5/A4 tests), --fails-without at the prototype (A4), fuzz ASan+UBSan (A6), probe strict
   before/after + A/A (A1, A2), identity (A3).
3. Note's before/after section (A7); push; CI (A8).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | |

## Result

## Rulings I made

1. Each chunk's complete elements arrive as one array (the shape the whole array had), not one object each as in
   the prototype: `DocIndexer::index` then expands it exactly as before, so a nested array element stays one
   element (refused as a non-object), and the first batch's front is an array, so no element's `_settings` or
   `_strict` becomes the index's (master never read them from an array body).
2. An empty array (any header form) is not streamed: its header goes to the unpacker, which yields `[]` as before
   (the indexer opens the index, as on master).
3. Elements are parsed at `MAX_JSON_DEPTH - 1`: the array is one of the body's levels, so the depth line is
   master's.
