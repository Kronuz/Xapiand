# journal-replaces-wal--chunks: journal

State: waiting on jobs 261121-115251-e9ce8c 261121-115253-91bb79 261121-115255-30ad58 261121-115257-10469a
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 1 (2026-11-21). Built (Xapiand:07e161f0b):

- `src/database/handler.{h,cc}`: `ProposeEntry` (the seam ShardEndpoint's propose fills, core step d),
  `chunked_id`/`chunked_term`/`ChunkedIds` (term in the high 32 bits, sequence from the ready point's log index),
  `ChunkedUpload` (body -> upload-stream chunk entries, each <= 1 MiB; `record()`; `final_entry()`),
  `abort_entry(id)` (an empty final), `propose_command` (whole when it fits, else staging chunks at `none` and the
  final at the level), `propose_upload`, `propose_upload_final`, `propose_abort`. A command any of whose entries is
  placed outside its id's term (or superseded) was cut by a leader change: errc::not_available, abort proposed.
  `upload_stored`/`index_uploaded` carry `SEAM(journal-replaces-wal core step d)` comments; no second write path.
- `tests/test_group_chunk.cc` (A3, through ShardApplication with a fake proposer that refuses > 1 MiB):
  chunk/final sizes and offsets, final applies, upload into its own volume, body too long/short -> ClientError +
  abort, leader change before the final -> not_available and nothing applied, first chunk in another term.
- `tests/test_group_chunk_group.cc` (A2, A3, a real lone-voter strata::group on PosixIO, command_bytes 1 MiB):
  whole proposal refused / chunked accepted; kill -9 (child `raise(SIGKILL)`) at every chore step, held journal
  completion and apply phase of a script (stored blobs, chunked stored blob, upload with a racing checkpoint,
  checkpoint, upload left open); the parent reopens: acked blobs byte-equal, present docs byte-equal, open
  commands retired by the new term, no orphan live file, and D16's recovery postcondition (each named file at its
  bundle length at the first entry after recovery).
- `harness/blob_crash_check.sh [--build DIR] [--mutant truncation]`.

Local build: build-chunks configured with FETCHCONTENT_FULLY_DISCONNECTED over .deps cloned from the core
worktree's build/_deps (strata efa5214b); jobs fetch their own.

## Next

1. Read the four jobs (at Xapiand:b376970a5): A3 plain 261121-115251-e9ce8c (test_group_chunk, test_group_chunk_group,
   test_shard_group, test_group_command); A2 plain 261121-115253-91bb79 (blob_crash_check.sh, every crash point);
   A2 mutant 261121-115255-30ad58 (--mutant truncation, --fails-without); A3 mutant 261121-115257-10469a
   (propose_command proposes the command whole, --fails-without). Paste `campaign brief`'s table.
2. Merge master once failed-restore-exit lands (handler.cc); never rebase.
3. With the lead's answer on Blocked-on 1: add the later-term chunk test beside the fix.

Local runs (light holds, build-chunks): test_group_chunk 92 checks OK; test_group_chunk_group OK at all 153 crash
points (4 min 23 s); the truncation mutant at stride 5 fails only D16's postcondition (vol-1 2190341 bytes on disk
where the bundle names 615424, and the like), never a read-back: ruling 1 measured.

## Blocked on

The lead (core's files, src/database/shard_group.cc, outside this task's set). Found while building against it:

1. **A chunk of a retired id in a later term fences every replica** (or, after the retirement's unlink, applies
   zero-filled bytes). Sequence at rf > 1: chunk c1 of id (term 1); the leader is re-elected at term 2, whose NoOp
   retires stage-id; the proposer's next chunk c2 lands in term 2: `chunk_applies` sees no open id and writes
   `stage-id`, which `file_set::write` refuses ("a retired file written again", logic_error): the replica fences,
   and replay repeats it. If a checkpoint unlinked the name first, c2 recreates the file with a hole [0, offset) and
   a final in term 2 would apply zeros. The handler stops at the first entry outside its id's term, but an entry
   already placed cannot be recalled. Recommendation: in `ShardApplication`, a chunk or a final whose
   `chunked_term(id)` differs from the entry's term applies nothing (the id names its term, rule 8); then
   `Open::term` is redundant. Test ready to add with the fix: chunks at term 1, `elect(2)`, one more chunk at term
   2, then the final: no throw, nothing applied, staging not live.
2. **An aborted upload's volume stays live**: an empty final (the abort) seals the upload volume instead of
   retiring it, so a client's abandoned 4 GiB upload keeps its volume forever. Recommendation: the final retires
   the upload volume when no blob of its command names it. `abort_entry()` is that empty final today.
3. **Open commands outlive a same-term reopen**: a close (D11) with an upload in flight resumes its term at reopen
   (closed record), so nothing retires the open command. Recommendation: the leader proposes `abort_entry(id)` for
   each id in `ShardGroupState::open` at its ready point (no proposer survives a close); ShardEndpoint's open, core
   step d.

## Evidence

| Criterion | Evidence |
|---|---|
| A2 | pending |
| A3 | pending |

## Result

<When ready for review.>

## Rulings I made

1. D16's truncation is invisible to a byte-equal read-back under kill -9: file effects are positional and a lone
   voter's log holds every applied entry, so replay rewrites every byte past the bundle's lengths identically
   (`bundle_model.py` has no truncation variant either). The A2 mutant is killed by the recovery postcondition the
   test checks at the first entry after recovery (each named file at its bundle length), not by a read.
2. The cut test is the placement term against the id's term, not the final's verdict: an empty transaction answers
   `accepted` with no docids, which a metadata-only final cannot tell apart from a cut.
3. Chunks go at `ack::none`, one at a time (no pipeline window): correctness first; the cadence measurement (F26)
   decides a window.
4. A cut answers errc::not_available (retryable, "retry it whole", D12), a body of the wrong length ClientError.
