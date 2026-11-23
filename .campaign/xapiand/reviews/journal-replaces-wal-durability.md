# journal-replaces-wal review: durability

Commit: Xapiand:3527100420f1b130ea650ea1e4aa6598305b466a
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The branch head moved to Xapiand:7ab94031a during the review; it adds only a test to `tests/test_pool.cc`
(read-your-writes, +30 lines), so every finding below holds there too.

## Evidence I relied on

- CI at Xapiand:a76e2ed61 (Build+unit+smoke, TSan, ASan+UBSan): green per the lead. `git diff --stat a76e2ed61
  3527100420` over `src tests harness CMakeLists.txt` is empty, so it stands for this commit's code.
- The lead's check of test_shard_group's count fix (a new Data holds the main object, src/database/data.cc:489).
- The contributor's resubmitted set, still pending, which I did not duplicate: the crash check 261123-152727-be5042
  (full ctest at Xapiand:7ab94031a, which runs `test_group_chunk_group`'s kill -9 at every crash point),
  261123-142143-9b5bc3 (test_shard_group under ASan), 261123-142149-92c842 (fails without the Data::flush fix),
  261123-142137-528ce7, 261123-142155-d17a82. Questions 2 and 3 below lean on them; if any fails, they reopen.

## What I checked myself

Code reading at the commit (file:line below) against strata 00eb018 (the pin), journal 9e11713 and completion
d8b538c (strata's pins). One rerunnable script, no jobs:
`Xapiand-wt/xapiand/journal-replaces-wal/.scratch/xapiand/review-journal-replaces-wal-durability/static_checks.sh`
(exits 1 while R1 or R2 is open; run it at the fix commit).

1. Acknowledgement (D4). Every write, metadata write, RESTORE batch and upload final is proposed at
   `ack::quorum` (src/database/shard.cc:501, :514, :811, :1141); only chunk entries and aborts go at `none`
   (handler.cc:2432, :2610), and no client answer waits on them alone. The HTTP answer comes after
   `write_document` returns, which needs `answer.applied` (shard.cc:520-526), so after the lone voter applied a
   committed entry; the journal makes a turn durable with `sync_data` (fdatasync, F_FULLFSYNC on macOS:
   completion posix.h:189-197) before it commits. I found no path that answers before then. `ack`, `read` and
   `refresh` are not parsed anywhere in src (G1), so today every write is `quorum`. `?commit` awaits the group's
   checkpoint (shard.cc:422-436). Spelling writes bypass the group, but strata refuses them
   (engine/strata/shard.cc:1255-1267).
2. Blobs and chunked uploads (A2, A3, D16). Apply writes positionally at offsets from the state
   (shard_group.cc:238-317); file effects run on every replayed entry, even ones strata skips (strata
   group.hh:690-716), and the log is cut only behind a published bundle (group.hh:948-950), so a strata commit
   by the catalog hook ahead of the bundle loses no blob bytes. Recovery truncates named files and deletes
   unnamed ones (file_set.hh:115-137). A lower-term open command is retired at the first higher-term entry,
   any kind (shard_group.cc:241-249, advance :436); a chunk or final outside its id's term writes nothing
   (:254-266). Uploads propose chunks one at a time and abort on any failure (handler.cc:2556-2600), and a
   final proposes once (shard.cc:507-515), so I found no way for a partial chunk set to apply. Sealing has a
   defect (R2).
3. Data::flush (Xapiand:09a223a4e). It builds new bytes into a separate buffer from the locators it reads,
   then `parse()`s them (data.cc:452-484), so every locator then views the Data's own bytes. Locator
   copy/move re-point `raw` at their own holder (data.cc:158-212), and a vector move of `pending` keeps its
   elements in place. Each codec pass in `place_stored` and the version stamp (shard_group.cc:139-165,
   :392-401) serializes from live bytes. I found no stale-view path.
4. Recovery and RESTORE. RESTORE prepares in the turn and proposes batches at quorum (shard.cc:1069-1170).
   The crash points around a checkpoint are `test_group_chunk_group`'s (on the real PosixIO and the real
   application, the journal's IO completions included). The old WAL logged replace, delete, set_metadata and
   spelling; the first three are commands now, and spelling is refused. The journal path is relative, but
   setup() changes into the data directory before the manager sets it (main.cc:719, :824-826).
5. `--full-flush-ordering` (D10): the index directories get the options (pool.cc:585, engine/strata/shard.cc:459);
   the journal host does not (R1).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | yes | defect | `--full-flush-ordering` never reaches the journal's IO, and the journal registers no barrier warning: the host's `PosixIO` is built with default options. The journal's one ordered sync is in `TurnLog::reclaim`, between removals of the oldest segments, so that a power cut undoes only the newest one. On macOS storage other than an internal Apple SSD that barrier is not guaranteed, and the switch that the warning names does not change it. A5 says the switch reaches the journal's IO, so A5 is not met. | src/database/shard_group.cc:556 (`std::make_shared<jn::PosixIO>(directory)`) against pool.cc:585; journal turn_log.h:436-450; completion posix.h:975-980; `static_checks.sh` R1 | Pass `engine::strata::posix_io_options()` to the host's `PosixIO` (opts are parsed before the manager builds it). Add a test that the host's IO carries the options: for example, a test seam that reads them back, or test_pool opening a pool with `full_flush_ordering` set and checking the host's IO. |
| R2 | yes | defect | Sealing a volume reads the whole file into one `std::string`: strata's `file_set::seal` runs `crc32c(read(name, 0, length))`. Apply seals every completed upload's volume (an upload's size, GiB for a large blob) and every rotated volume (up to `VOLUME_BYTES`, 32 GiB). A 32 GiB rotation allocates 32 GiB on the chore, so it can fail with `bad_alloc`. That error is not an ENOSPC stall, and replay seals again at the same entry, so the shard can never reopen. Master's storage library never buffered a whole volume. | strata include/strata/group/file_set.hh:65-69 and :48 (same on strata origin/main); Xapiand shard_group.cc:298-300 (rotation seal), :303-308 (upload seal), shard_group.h:59 (`VOLUME_BYTES`); `static_checks.sh` R2 | In strata, make `seal` use the streaming `crc32c_of(file, length)` that recovery already uses (file_set.hh:149-159), then bump the pin. Test: seal a file larger than a bounded allocation (strata unit test, or test_shard_group with an upload of several MiB under an allocation cap). The lead routes the strata half. |
| G1 | yes | gap | Not in this commit: the HTTP `ack`, `read` and `refresh` levels. Nothing in src parses them (`git grep` finds no `ack=` handling, and search_views.cc has only `commit`), and the `levels` task has not run. That makes A5 and A4/A6 incomplete. Every write is `quorum` today, so nothing is acknowledged early, but `ack=none` (answered at placement) and RESTORE at `none` (lead ruling 2026-11-21) have not been reviewed. | src/server/search_views.cc:107, :207; `campaign status journal-replaces-wal` (A5 pending, tasks: chunks and restore only) | Finish the `levels` task with its tests. Then run a durability pass over that diff alone: an `ack=none` answer before placement fails, and a `none` RESTORE reports no docid. |
| N1 | no | defect | A RESTORE slice logs the id counter in its own command after its batches. A `kill -9` between the two leaves the counter behind ids that are already durable. Then the next issued POST id conflicts (409, by `expected_version = 0`, shard.cc:734-736), and a RESTORE batch write, which carries no expectation, can overwrite a document from the crashed, unacknowledged RESTORE. No acknowledged write is lost. | src/database/shard.cc:1154-1168; restore_batch.cc (`write.expected.reset()`) | Append `id_high_write` to the slice's last batch command, as `write_document` does (shard.cc:625-629). |
| N2 | no | nit | An upload's final seals the upload volume without checking that its length equals the record's size. In-order placement within a term makes a short volume impossible today; the check would catch a future regression at apply, where today it surfaces only at read (`read_stored`'s `corrupt`). | src/database/shard_group.cc:301-309 | Compare `files.length(volume_name(upload))` with the record size before the seal; a mismatch is a corrupt apply. |
| N3 | no | nit | Once the catalog's commit hook (`commit_blocking`) has run, `modified` is false, so a later `?commit` returns without the group's checkpoint: no bundle, no log cut. Durability is unaffected because the write is in the log, but D4's "explicit durability point" becomes a no-op then. | src/database/shard.cc:317-343, :363-366, :424-426 | Have `commit_group` track the group's own applied/published positions rather than `modified`, or document the behavior. Owner's call. |
| N4 | no | gap | Not probed: whether a client can address `/.xapiand/journal/...` as an index path, which would open a strata index inside the journal host's directory. `.xapiand/indexes` is addressable as a system index. | name_index.cc:49 skips dot segments only when listing | A test that a write to `.xapiand/journal` is refused, if it is not. |

## Not run

- No probe jobs: R1 and R2 are settled from the code, and the crash, ASan and fails-without evidence is the
  contributor's pending set above, which I did not duplicate. Pending probes: none.
- Power loss (as opposed to `kill -9`, which keeps the page cache) is not covered by any job here; the quest
  defers it to `--later` (`power_loss_check.sh`, dm-log-writes). Consensus's "committed only after the turn
  is durable" at rf = 1 is strata/consensus's contract (group-core's review), which I took as given.
- ENOSPC on the journal host's own volume, and a lost `.xapiand/journal` directory (a group whose index has a
  bundle but whose journal is gone), were not exercised.
- I am a subagent of the lead's session, not a session of my own, so no `xapiand · reviewer` title applies.
