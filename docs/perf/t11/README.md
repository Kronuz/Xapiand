# Gate 7a T11: the strata engine share per document

T11 asks that the engine rows of the profile ("replaced S1") cost at most 0.5x Xapian's per document, in CPU and on the
critical thread, on the same bulk RESTORE (`harness/profile_bulk.sh`, 20000 accounts documents, one fresh `--solo` node
per run, `sample(1)`). It was 0.80x CPU and 0.53x critical path.

Result: 0.47x CPU, 0.22x critical path (five interleaved rounds each, `quiet exclusive`).

## Final comparison (macOS x86_64 APFS, RelWithDebInfo with frame pointers, master `fade84b1e` as the base)

CPU is the smoke run's CPU per document times the engine rows' share of the sampled profile, as the gate defines it.

| Metric | Xapian | Strata, master | Strata, T11 | T11 / Xapian | master / Xapian |
|---|---|---|---|---|---|
| Engine CPU per document (ms) | 0.375, spread 6.3% | 0.314 (A/A 0.310), spread 5 to 8% | 0.176, spread 6.8% | 0.468 (pairs 0.470 0.458 0.481 0.476 0.456) | 0.836 |
| Engine CPU per document, from samples (ms) | 0.399, spread 4% | 0.325 | 0.173, spread 10% | 0.434 | 0.816 |
| Engine on the critical thread (ms/doc) | 0.0748, spread 8% | 0.0423 (A/A 0.0440), spread 22 to 26% | 0.0166, spread 10% | 0.222 (pairs 0.238 0.212 0.233 0.219 0.208) | 0.567 |
| Whole process CPU per document (ms) | 1.141, spread 2% | 0.838 | 0.780, spread 4% | 0.683 | 0.735 |
| Docs/s | 4474 | 5188 | 6355 | 1.42 | 1.16 |
| Peak RSS after the RESTORE (MB, 3 runs) | 916 to 928 | 1160 to 1169 | 833 to 855 | 0.92 | 1.26 |
| Index directory after `?commit` (KB, files) | 12868 to 12908, 30 | 1760, 20 | 1760, 20 | 0.14 | 0.14 |

All four axes: time and throughput improve, memory improves (the flushes and merges that held buffers are gone), disk is
unchanged. Note the whole-process CPU falls by 7% (0.838 to 0.780) while the engine rows fall by 44% (0.314 to 0.176):
the rows the profile counts include blocked time (`F_FULLFSYNC` under `commit_files` counts as busy), and when the
writer is faster the other rows (schema, pipeline glue) take more samples per document. Do not read the engine row's
44% as 44% of the process.

## Experiments

Each is interleaved base / new / base on the same machine, so the A/A is the second base against the first. Ratios
are new over base on the engine CPU row (smoke CPU x share) unless stated.

| # | Change | Base | New | Ratio (pairs) | A/A | Critical thread | Kept |
|---|---|---|---|---|---|---|---|
| E1 | `StrataDocument` as flat vectors with a hashed term table, found by the host's field id (`e1-flat-document.md`) | 0.310 | 0.276 to 0.285 | 0.89 / 0.90 (0.90 0.87 0.88 0.91) | 1.00 | 0.043 to 0.037 | yes |
| E2 | `replace` and `erase` read the version through strata `find_docid`, not `locate` (flush + merge) (`e2-replace-without-flush.md`) | 0.322 | 0.230 | 0.72 (0.64 0.72 0.75 0.75) | 1.01 | 0.041 to 0.024 | yes |
| E3 | the shard vouches for the terms it fills (`field_part::checked`) (`e3-trusted-terms.md`) | 0.236 | 0.224 | 0.95 (0.92 1.01 0.99 0.88) | 1.03 | 0.0239 to 0.0202 | yes, marginal |
| E4 | term bytes and positions in two per-document arenas (`e4-document-arena.md`) | 0.226 | 0.187 | 0.83 (0.84 0.80 0.82 0.85) | 0.96 | 0.0219 to 0.0176 | yes |
| E5 | one term table per document (`e5-document-term-table.md`) | 0.197 | 0.168 | 0.86 (0.67 1.01 0.82 1.01 0.78) | 0.99 | 0.0185 to 0.0152 | yes, noisy |

No experiment lost. E3 and E5 are inside the spread of their own A/B (E3 by CPU; its critical-thread gain, 15%, is
larger than its A/A); they are kept on mechanism (the sort and 47 per-field vectors are gone) and on the final
five-round comparison above, which is the claim.

E2 is the largest win and the surprise: a `Shard::replace` of a document still in the write buffer called `locate`,
which reads flushed segments only, so it flushed the buffer to a tiny segment and ran the merge policy inline on the
writer thread (about 1300 samples of a 6100-sample row). The version check needs only the version, which `apply()`
already reads from the buffer's docid map; `find_docid` exposes that lookup.

## Where it is not done

- Measured on one host (macOS x86_64, APFS). The changes are portable C++ (no platform branches); Linux, FreeBSD and
  arm64 are not measured. Whether `F_FULLFSYNC` waits (about 600 of the remaining 3500 row samples) count as engine CPU
  on the other platforms depends on the harness.
- The remaining row: `commit_files` fullsync waits about 17%, `intern` about 10%, `fill` and the destructors about 10%,
  the flush's tuple buffer on the autocommit thread about 15%. Next: fold the first directory barrier of a commit into the
  data batch (completion `PosixIO`, the "not yet done" item in `PLATFORM_FINDINGS.md`: one device flush fewer of four per
  commit), which belongs with the Detent owner of that library.
- Shard writable-checkout wait on the critical thread was 49.8% and is 7c's; E2 lowered it (43% in one pair) because the
  writer holds the lock for less time. It is not counted in T11.

## Correctness

Docs E2E on strata: 566 of 566 assertions pass before and after; `bodydiff.py` before against after shows only the
volatile bodies (random ids, `GET /`, `:metrics`); `search_order_diff.py` 92 of 92 identical in order, scores, total
and count. `strata_callbacks_check.py`: 210 searches, 0 different. `strata_write_check.sh write replay versions`: no
failure. Unit tests engine_fields, engine_strata, engine_shard, engine_replay, engine_document, engine_search_strata
pass; strata's `write_buffer_reads_test` (new `find_docid_reads_buffers`) passes.

## What it saves

At the final numbers the whole process uses 0.058 ms less CPU per document (0.838 to 0.780): about 16 CPU-hours per
billion documents indexed, which at roughly $0.04 per core-hour is under a dollar per billion documents. The engine row
itself is 0.138 ms per document lower (38 CPU-hours per billion), but part of that moved to other rows, so the process
figure is the honest one. The real effect is throughput of a single bulk load on one host: 1.22x the master strata rate
(6355 against 5188 docs/s) and 1.42x Xapian's.
