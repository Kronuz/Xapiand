# Gate 0.5: the bulk-index slowdown and the estimate change

Question: which commits make bulk indexing 19% slower (3124 vs 3860 docs/s in the gate) and change the
`SEARCH /bank/` `total` estimate from 459 to 400. Base `855f75281`, head `cca0011bb`. macOS x86_64
Release+LTO, `quiet strict` holds (a strict hold acts as exclusive on this Mac), 20000-doc RESTORE into a
fresh `--solo` node per run, interleaved rounds.

## Estimate: 459 to 400 is `db0be79ad`, a correct fix

Bisect on first-parent, then inside the merge (each step built and run on the `_and` gender female,
age 20..30 search after a RESTORE of accounts.ndjson):

| Commit | total |
|---|---|
| `855f75281`, `fe9307227`, `566943e9e`, `201414347` | 459 |
| `60efd75ef` (merge of engine-query), `1ab0ce077`, `cca0011bb` | 400 |
| `801f16f5c` (engine query step 1), `a0926598b` | 459 |
| `db0be79ad` "Fix the engine query review findings CR1 to CR5" | 400 |

CR2 in that commit: the range posting source adapter set its termfreq and weight bounds in `init()`,
which Xapian does not call when it binds a `ValuePostingSource` (it calls `reset()`), so the bounds never
reached the matcher; they are now set in `reset()` (`src/engine/xapian/callbacks.cc`
`PostingSourceAdapter::reset`, `set_termfreq_min(0)` plus the weight bound). Before the seam, and in the
seam's first commit, the filter kept the default (value-slot) frequency bounds; now it states that it may
accept no document. `count` and `_doc_count` (232, the true match count) are unchanged. 400 is closer to 232
than 459, so the new number is the intended behaviour, not a bug; the gate should record it as an intended
change. The estimate stays an over-estimate.

## Bulk index: `7eae6786c`, "Add the engine seam" (first commit of the engine-core merge)

Steady 5 to 6 percent run-to-run spread in the A/A. Docs/s per run, mean of 4 or 5 interleaved rounds
(min..max), CPU is user plus sys of the node for one 20000-doc run:

| Binary | docs/s | CPU user/sys s |
|---|---|---|
| base `855f75281` | 4837 (4816..4860) | 16.7 / 5.2 |
| base again (A/A) | 4644..4826 (one 311 stall run) | 17.0 / 5.5 |
| `5cce4860d` (parent of the seam) | 4842 (4778..4932) | 16.3 / 5.2 |
| `7eae6786c` (seam types, Schema/GenerateTerms/Shard write engine::Document) | 3886 (3860..3914) | 17.6 / 5.7 |
| `3be508883` (engine::Shard writes) | 3808 | |
| `566943e9e`, `2466dac5c`, `44f628903`, `201414347` | 3750..3850 | |
| `cca0011bb` | 3774 (3702..3841, 10 runs) | 17.7 / 5.6 |

7eae6786c is 0.80x its parent (3886 vs 4842, 4 to 5 pairs, ranges do not overlap); nothing later moves it.
Earlier steps (`fe9307227`, `0f4424eb6`, `5cce4860d`) equal base. The CPU rise is only +8% user and +7% sys,
so the wall loss (+25% time) is a serial section, not extra parallel work.

Mechanism. The write path builds an `engine::Document` (std::map of terms with std::set positions) and
converts it with `engine::xapian::to_xapian` (`src/engine/xapian/convert.cc:50`) into a `Xapian::Document`
at the database call, which is now inside the shard's write section (`lock_shard` in
`DatabaseHandler::replace_document_term`, `src/database/handler.cc`, then
`Shard::replace_document_term` and `engine::xapian::Shard::add/replace/replace_by_key`,
`src/engine/xapian/shard.cc`). Before, `Schema::index` built the Xapian document in the 16 parallel client
threads. A `sample` of the node (60000-doc load) shows `Schema::index` inclusive falling 3669 to 1807 and
`GlassWritableDatabase::replace_document` 4956 to 3053 while `to_xapian` adds 1613: cost moved from the
parallel part to the per-shard serial part, and the document is built twice (engine then Xapian, plus again
in `Document::serialise` for the WAL, on the WAL thread). Profiles: `.scratch/bisect05/prof-*.txt` in the
main checkout.

## Experiments (3)

1. Convert before the hop in `engine::xapian::Shard`, replay the version/finalise calls onto the converted
   document. LOSER: 3801 vs 3766 docs/s (+1%). The conversion still ran inside `lock_shard`.
2. Convert before `lock_shard` in `DatabaseHandler::add_document/replace_document/replace_document_term`
   (`engine::xapian::prepare`), `Document` logs calls made after the conversion (`begin_tail`) and the shard
   replays them (`finish`). WIN: 4476 (4420..4518, 5 runs) vs `cca0011bb` 3774 (3703..3841, 10 runs):
   1.19x faster, recovers 73% of the gap; the parent is 4793 (4551..4910), so 0.93x of it remains.
   Identity: a full SEARCH dump of the 1000-doc bank restore (all hits by id, 4 pages) and `:info` hash
   identical to `cca0011bb` and base; the estimate stays 400.
3. Not run: the remaining 7 percent is probably the engine::Document build (map/set nodes, nanov2 malloc) in
   `Schema::index` and the WAL's second conversion.

The prototype is rough (type-erased `prepared_` in `engine::Document`, a mutation log, a `prepare` call in the
handler, which `seam_check` may flag). A proper fix is to make the engine own an opaque, pre-built write
handle (`engine::Shard::prepare(Document&)`), or to let `engine::Document` build its Xapian form directly
when the engine is Xapian.

## Side finding

About 1 run in 40, any binary including base, a RESTORE stalls for about 64 s (311 docs/s) with the
node idle. Not from the seam; worth a separate look.
