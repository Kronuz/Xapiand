# restore-stream: journal

State: waiting on jobs 261121-202756-2c7fb6 (A6), 261121-202756-ca0d1d (A3), 261121-202757-d2afb8 + 261121-202758-e5258b (A1), 261121-202758-fe2fcf (A2), at merge head a076cc91f0a9; CI there needs the push. Master merge voided the security acceptance (21 code files under review); lead re-accepts after reading `git diff 0547322a4d72 78484bdee355` and these jobs.
Quest: .campaign/xapiand/quests/restore-stream.md (the lead's; never edit it)

## Doing

Jobs at 5842f9ef3 all finished green. A1/A2 reruns replaced the note's table (before: logs 1,322 -> 333 MB, 9.79 /
9.59 / 9.90 s; shakespeare 1,846 -> 423 MB, 13.43 / 10.21 / 11.25 s; after: logs 1,357 -> 344, 8.47 / 9.19 / 7.98;
shakespeare 1,860 -> 405, 11.21 / 7.65 / 9.66). Memory cut unchanged within 4%; the times are a wash on logs.

## Next

1. Lead: land once CI is green at the merge head.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-202757-d2afb8, 261121-202758-e5258b |
| A2 | 261121-202757-d2afb8, 261121-202758-e5258b, 261121-202758-fe2fcf |
| A3 | 261121-202756-ca0d1d |
| A4 | 261121-034148-685f8b (passed at c5b2c0689ce5; 5 merges from main since, none touching its files; ignores docs-only docs/perf/bulk-load-rss.md); fails without: 261121-034201-4acbcc, 261121-034203-69a143 |
| A5 | 261121-034148-685f8b (passed at c5b2c0689ce5; 5 merges from main since, none touching its files; ignores docs-only docs/perf/bulk-load-rss.md) |
| A6 | 261121-202756-2c7fb6 |
| A7 | `docs/perf/bulk-load-rss.md`: "With the body streamed, the peak is 344-405 MB, held mostly by strata's write buffer until the RESTORE's one closing" |
| A8 | CI green at e3265faf5147 (run 44949695201); no longer waits on lua-method-flake |

## Result

The change: Xapiand:a131ad2b2 (src). Tests: tests/test_restore_stream.cc (answers: master's at e1be368395a1 from job
261121-020515-7502ec; the D162 rows from the change's capture 261121-020516-e55b7e), tests/test_restore_stream_fuzz.cc.
Note: docs/perf/bulk-load-rss.md. Probe: harness/bench/bulk_rss_probe.py.

## Rulings I made

1. Each chunk's complete elements arrive as one array (the shape the whole array had), not one object each as in
   the prototype: `DocIndexer::index` then expands it exactly as before, so a nested array element stays one
   element (refused as a non-object), and the first batch's front is an array, so no element's `_settings` or
   `_strict` becomes the index's (master never read them from an array body).
2. An empty array (any header form) is not streamed: its header goes to the unpacker, which yields `[]` as before
   (the indexer opens the index, as on master).
3. Elements are parsed at `MAX_JSON_DEPTH - 1`: the array is one of the body's levels, so the depth line is
   master's.
4. A3's first failure (261121-020725-ebb489) was the probe, not the engine: es-logs.msgpack holds no `_id` key (0
   occurrences of the fixstr `_id`; logs-2.json's action lines carry none), while es-shakespeare.msgpack holds
   111,396, one per element. Both logs restores dumped 14,005 documents of identical total size (52,869,909 bytes)
   with every id differing: generated ids. The probe now compares logs as sorted documents without `_id` and
   checks each restore's ids distinct; shakespeare keeps the exact id-and-content comparison. A content difference
   still fails the job.
5. A4's applied elements differ from NDJSON's, stated as the quest allows: elements of chunks before the failing
   one are restored, as NDJSON's lines before a bad one (`fewer`: a and b stay, total 2); the failing chunk's
   elements and everything after are not, where NDJSON skips a bad line and applies the rest (`more`,
   `trailing`, `invalid`, one chunk each: total 0).
