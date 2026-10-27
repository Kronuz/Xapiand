# seam-write (phase 0.5, bulk-index recovery)

Branch `phase0.5/seam-write` from Xapiand master `8170a3ef6`. Local only, not pushed.

## State

Done and measured. `engine::Document` is a handle over the engine's own document (a `Xapian::Document` for
Xapian), so Schema builds the native document in the parallel stage and the shard and the log use it
unchanged. Mechanism note: `docs/mechanisms/engine-write-path.md`.

## Done

1. Native-backed `engine::Document`; shard, log and word-break terms use it (commit below).
2. Proof: bulk A/B against `5cce4860d` and `cca0011bb` (6 interleaved rounds plus A/A), SEARCH dump and
   `:info` identical to `cca0011bb`, docs E2E 576/576 (newman 6.2.3) on Release and ASan+UBSan, 18/18 unit
   tests under ASan+UBSan, a 20000-document bulk under ASan, WAL recovery check, `seam_check` 7 files and 158
   lines (unchanged), a mutant (`add_boolean_term` as `add_term`) fails 536 of 645 document checks.
3. Revert check: `cca0011bb` is the reverted state, 3704 docs/s against 4635.

## Next

1. The remaining 3 percent is the engine shard's coroutine and `sync_wait` layer on the write and the key
   lookup; a no-hop fast path in the task library's `sync_wait` is the next lever.

## Blocked on

Nothing.
