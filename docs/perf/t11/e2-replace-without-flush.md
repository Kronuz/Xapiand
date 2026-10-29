# T11 E2: replace by docid without flushing the index

Target: the strata engine row of gate 7a T11 (see e1-flat-document.md).

## Footprint

In the E1 profile, `Shard::replace` -> `locate` -> `basic_index::refresh` -> `flush` / `maybe_merge` is the largest
remaining item on the writer threads (DI, under the shard lock, so on the critical path): `flush_active` 531 samples,
the inline merge it triggers 698, `reclaim_files` 72, about 1300 of a strata row of about 6100. Counting at the call
sites (a temporary counter, removed): fewer than 2000 `locate` calls and 500 to 1000 flushing refreshes for the
20000 documents. `locate` reads only flushed segments, so a replace of a document that is still in the write buffer
flushes it first, writes a segment, and runs the merge policy after it.

## Hypothesis

`replace` and `erase` need only the stored version of the document (to check it and number the new one). The buffers
already answer that: `apply()` looks the docid up in the active buffer's docid map and the segments. An additive
`basic_index::find_docid(docid)` (strata) that does the same lookup with no flush removes the flushes, the tiny
segments and the merges they provoke, with the same result: the same version, from the newest live copy. `get` and
`find` paths that need the segment (`locate`) stay as they are.

## Procedure

1. strata test `find_docid_reads_buffers` (buffer, flushed, erased, no segment written by the call).
2. Unit tests engine_strata, engine_shard, engine_replay, engine_search_strata.
3. Docs E2E against the base: assertions and `bodydiff.py` unchanged.
4. T11 profile, 4 interleaved base / new / base pairs under `quiet exclusive`.

Keep if the strata engine row falls beyond the A/A spread and no other axis (disk: segment count and bytes after the
RESTORE; memory: peak RSS) gets worse.
