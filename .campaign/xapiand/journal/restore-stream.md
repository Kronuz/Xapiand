# restore-stream: journal

State: waiting on jobs 261121-034148-685f8b 261121-034201-4acbcc 261121-034203-69a143 261121-034214-a0dea9 261121-034215-99dd61 261121-034217-cc2491, and CI at 5842f9ef3
Quest: .campaign/xapiand/quests/restore-stream.md (the lead's; never edit it)

## Doing

Head Xapiand:5842f9ef3 (pushed): expected answers pasted, probe's identity takes `--ids generated`, note's
before/after and remaining-peak sections written. Jobs queued at it:
- 261121-034148-685f8b: the RESTORE stream tests, feed and node (A4, A5)
- 261121-034201-4acbcc, 261121-034203-69a143: `--fails-without` at the prototype b60d09d64 with the tests from
  5842f9ef3, FILTER=fewer and FILTER=more (A4)
- 261121-034214-a0dea9: identity rerun, both binaries; shakespeare ids exact, logs by content without `_id` (A3)
- 261121-034215-99dd61 (logs), 261121-034217-cc2491 (shakespeare): A1/A2 rerun, same command; the probe file
  changed, so 261121-020723-9399ae and 261121-020724-73f7ef no longer count at head

## Next

1. When the jobs pass: if the A1/A2 reruns' medians differ from the note's table (from 9399ae/73f7ef), put the
   reruns' numbers and ids in docs/perf/bulk-load-rss.md; fill the Evidence table; push.
2. CI green at head (A8): the only CI failure at 7df95f564 was test_restore_stream's empty expected table.
3. State "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-034215-99dd61, 261121-034217-cc2491 (pending; 261121-020723-9399ae, 261121-020724-73f7ef passed with a131ad2b2's probe: logs 1,322 -> 333 MB, shakespeare 1,846 -> 423 MB) |
| A2 | same jobs (pending; earlier medians: logs 9.79 s master, 9.59 change, 9.90 A/A; shakespeare 13.43, 10.21, 11.25) |
| A3 | 261121-034214-a0dea9 (pending) |
| A4 | 261121-034148-685f8b, 261121-034201-4acbcc, 261121-034203-69a143 (pending) |
| A5 | 261121-034148-685f8b (pending) |
| A6 | 261121-020737-3fde05 |
| A7 | `docs/perf/bulk-load-rss.md`: "With the body streamed, the peak is 333-423 MB, held mostly by strata's write buffer until the RESTORE's one closing" |
| A8 | CI at 5842f9ef3 (pending) |

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
