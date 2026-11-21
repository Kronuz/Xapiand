# restore-stream: journal

State: waiting on jobs 261121-020515-7502ec 261121-020516-e55b7e 261121-020606-cadac7 261121-020556-58a126 261121-020723-9399ae 261121-020724-73f7ef 261121-020725-ebb489 261121-020737-3fde05
Quest: .campaign/xapiand/quests/restore-stream.md (the lead's; never edit it)

## Doing

Commit A (Xapiand:879d1eac6: tests, note, probe; src = master) and commit B (Xapiand:a131ad2b2: the change).
Jobs queued:
- 261121-020515-7502ec: capture master's answers (A, `node` tests, RESTORE_STREAM_CAPTURE=1)
- 261121-020516-e55b7e: capture the change's answers + feed unit tests (B)
- 261121-020606-cadac7, 261121-020556-58a126: Release xapiand at e1be368395a1 / a131ad2b22b6 into
  ~/.scratch/xapiand/restore-stream/xapiand-<sha>
- 261121-020723-9399ae (logs), 261121-020724-73f7ef (shakespeare): strict, 5 interleaved rounds of
  before / after / before again (A/A), array body (A1, A2)
- 261121-020725-ebb489: identity, array vs stream DUMPs, both binaries (A3)
- 261121-020737-3fde05: ASan+UBSan fuzz, 4 seeds x 480 s (A6)

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
