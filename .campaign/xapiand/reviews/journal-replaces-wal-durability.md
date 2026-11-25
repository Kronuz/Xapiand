# journal-replaces-wal review: durability

Commit: Xapiand:0ea6bd04aaf47c8505d90c55d5997a193d2269b6
Verdict: CHANGES
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Round 2, a fresh review of the whole mechanism. The branch head Xapiand:33c922879 adds only `.campaign/` files
after this commit (`git diff --stat 0ea6bd04a 33c922879 -- . ':!.campaign'` is empty), so every finding holds there.
Round 1's blocking findings are resolved: R1 (probe S below, and 9c2628's IO-options case), R2 (7adbab:
`test_a_seal_reads_its_volume_in_bounded_pieces`, 128 checks passed, at the strata 7a4284c4 pin, whose
`file_set::seal` streams its CRC in 64 KiB reads) and G1 (the levels, 9c2628 and 82cbcd; ack=none answers 202).
Four new blocking defects came out of the probes.

## Evidence I relied on

- CI at Xapiand:0ea6bd04a: the `CI` run on `quest/xapiand/journal-replaces-wal` concluded success (`gh run list --commit`).
- The contributor's jobs at this commit, all `done 0` (the mutant `done 1`, killed): the crash check 261124-160741-26cfd4
  (full ctest, including test_group_chunk_group's `kill -9` at every crash point on the real PosixIO); no-wal
  261124-160741-838232; A2 261124-160800-54a48e (`blob_crash_check.sh`: stored blobs and chunked uploads around a
  checkpoint); R2 261124-160800-7adbab; A1/A4/A9 261124-160758-92f823 (aborts at the ready point, cut chunks);
  A3 261124-160758-2e2d34 and 261124-160800-a73a91; A4 261124-160759-3536d4; A5 261124-160801-9c2628 (levels,
  RESTORE at none, the journal IO's options) and 261124-160801-82cbcd (journal replay, the failure naming its file);
  the abandon case 30x 261124-160801-40491e and the drain case 30x 261124-160758-b7c564; the mode mutant
  261124-160810-feba37 (the host's IO opened plain fails). Together they cover `kill -9` recovery, chunking,
  levels and the shutdown, so I did not rerun any of them.

## What I checked myself

Code reading at this commit against strata 7a4284c4, consensus 6e1122cc, journal fc5c59b and completion b4600a8,
and six probe jobs at this commit (`campaign job submit --quest journal-replaces-wal --commit 0ea6bd04aaf4`).
Scripts and their job wrappers (each `job-*.sh` header has its submit line, to rerun on the fix) are in
`Xapiand-wt/xapiand/journal-replaces-wal/.scratch/xapiand/review-journal-replaces-wal-durability/`:

| Job | Script | Result |
|---|---|---|
| 261124-190201-acf64e | `job-node.sh`: `logcap_probe.py` (L), `syncshape_probe.py` + `syncspy2.c` (S) | L fails 5 of 5 cases (L1); S passes in both modes |
| 261124-190906-60b5b8 | `job-after.sh`: `after_probe.py` | restore and burst fail (L1); no wedge; index DELETE 501; its damage and moved cases were invalid, replaced by ffc17f |
| 261124-191001-d069b2 | `job-fullcrash.sh`: `fullcrash_probe.py` | passes: `kill -9` with a shard's log at the cap keeps 170 of 170 acknowledged documents, and writes resume |
| 261124-191056-51e8cd | `job-journalpath.sh`: `journalpath_probe.py` | fails: client writes put `strata/` directories inside `.xapiand/journal` (round 1's N4) |
| 261124-192149-c90be0 | `job-journalname.sh`: `journalname_probe.py` | fails 2 of 4: the node never starts again (J1) |
| 261124-191523-ffc17f | `job-refusal.sh`: `refusal_probe.py` | damage: the node aborts (D1); lost: blobs deleted, another document's blob served (J2) |

1. **Acknowledged writes, their levels, crash and power cut.** A `quorum` write is answered after the lone voter
   applies its committed entry. An `ack=none` write is answered at `ProposalPlaced`, which consensus defers until the
   entry's `Persisted` (consensus core.h:1042-1045), so it is in the durable log when it is answered (202). Chunked
   commands acknowledge only at their final entry. Their chunks precede it in one log, and a cut command applies
   nothing (A3, A9 jobs). Blob records are rewritten by replay from the last bundle: effects run before strata's
   verdict (strata group.hh:877), and strata skips entries at or below its head (:928). The log is cut only behind a
   published bundle. A RESTORE slice's last command carries the id counter (src/database/shard.cc:1118-1124,
   ruling 43). `kill -9` is covered by 26cfd4, 54a48e and d069b2: d069b2 kills the node with a shard's log at its
   16 MiB cap, and all 170 acknowledged documents read back. Power cut: full mode flushes in full at every sync (2
   below). Plain mode on macOS syncs with `fsync`, which a power cut may undo, as options.md:276-280 states. No job
   cuts power to a node's journal and indexes together (see Not run).
2. **`--full-flush-ordering` reaches every IO, and plain never reaches a full path.** Probe S interposed fsync,
   fdatasync and fcntl in a node over writes, an `ack=none` write, a 3 MiB chunked blob, a RESTORE, a delete, COMMIT
   and SIGTERM. In full mode it saw 260 `F_FULLFSYNC` across the journal (217), strata (40) and the file set (3),
   0 barriers, and every fsync run on a thread ended in an `F_FULLFSYNC`. In plain mode it saw 277 fsync and 10
   barriers (strata), with 0 `F_FULLFSYNC`. All three `PosixIO` constructions take `posix_io_options()`
   (shard_group.cc:561, pool.cc:599, engine/strata/shard.cc:469).
3. **Lifecycle.** The group of one index, state by event:

   | State | Event | What happens | Safe? | Evidence |
   |---|---|---|---|---|
   | no group in the journal | open | `driver.create`, then adopt of an empty bundle: the file set's recover deletes every `app.*` file (strata group.hh:262, :1152, :1183; file_set.hh:135) | a new index: yes; an index that holds state: **no** (J2) | ffc17f lost |
   | stored or closed | open | recover from the last bundle; P_b > P_H fences; the file set truncates to the bundle; damage opens the group damaged and arms its repair (group.hh:279) | a sound bundle: yes; damage: **no** (D1) | 26cfd4, 54a48e; ffc17f damage |
   | opened | catch-up | a read barrier through the commit index (replay), then open chunked ids and the id counter on the chore (shard.cc:265) | yes, unless damaged (D1) | 92f823, 3536d4, d069b2 |
   | caught up | abort of in-flight uploads (A9) | an abort at `none` per open id (shard.cc:274); a refused abort waits for the next term or the next open | yes | 92f823; 60b5b8 (the refused upload's volume is gone after a clean stop) |
   | ready | write at `quorum` or `none` | placed, persisted, answered at placement (`none`) or after apply (`quorum`) | durable at its level; **refused past 16 MiB of log** (L1) | 9c2628, 82cbcd, d069b2; acf64e |
   | ready | chunked command, streamed upload | chunks at `none`, the final at its level, an abort on failure, a term change cuts | yes; **uploads past about 15 MiB refused** (L1) | 2e2d34, a73a91, 92f823; acf64e |
   | ready | COMMIT, autocommit, a RESTORE's closing commit | the turn, then `commit_group` checkpoints on the group's loop: capture between entries, sync files and strata, publish, cut the log, unlink retired files; nothing while `modified` is false (N3) | yes | b7c564, 40491e; 60b5b8 |
   | ready | catalog commit hook | `commit_blocking` is a plain strata commit, never a checkpoint (pool.cc:644-646, shard.cc:317-343); strata ahead of the bundle loses nothing, since replay reruns file effects | yes | code |
   | ready | index DELETE | 501, as on master (search_views.cc:1550; master's :1555) | yes | 60b5b8 |
   | ready | close | `close_payload` → `group.close`: checkpoints until the bundle covers the commit index, keeps the closed record; a failure leaves the log and the bundle | yes | 26cfd4 (test_pool journal cases) |
   | any | `kill -9` | the journal replays from the last bundle | yes, at the cap too | 26cfd4, 54a48e, d069b2 |
   | any | power cut | full: `F_FULLFSYNC` at every sync; plain: `fsync`, lossy on macOS (stated) | not run at this level | acf64e (calls only) |

4. **Seal and refusals.** The seal is bounded (R2, above). Refusals mostly do not name their cause. A full log answers
   500 "consensus call rejected" and drops the reason (`LogFull`) (L1). A damaged file set aborts the node before any
   line names the damage (D1). An index whose group is gone answers "open existing journal file: No such file or
   directory" for its blob volume (J2). A journal that cannot be recovered names the directory and errno, not the
   offending entry (c90be0). The journal replay's own failure names its file (82cbcd).
5. **Weaker than master, and whether it is stated.**
   - Stronger: an answered write is durable at its level. Master's WAL fsync was asynchronous (master wal.cc:108,
     through the fsyncher).
   - Weaker:
     1. A shard takes at most 16 MiB of log between checkpoints (L1). Master's WAL had no such bound. Not stated.
     2. An index directory is no longer self-contained. Its committed blobs depend on its group in
        `.xapiand/journal` (J2). Not stated.
     3. One damaged volume makes the whole index refuse at rf = 1, once D1's abort is fixed. Master served the rest
        of the index. Not stated (N8).
   - Plain mode's power-loss window on macOS is stated in options.md:276-280.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| L1 | yes | defect | **A shard's group log is capped at 16 MiB between checkpoints, and nothing checkpoints when the log fills.** Consensus refuses a proposal past `log_bytes` (`LogFull`), which shard_group.cc:524 sets to `16 * COMMAND_ENTRY_BYTES`. Checkpoints run only from a commit while `modified` is set (autocommit 1 s after the last write, forced at 8-10 s) or at close. On a quiet 1-shard index: (1) streamed uploads of 18 MiB and 40 MiB answer 500. The changelog promises streamed uploads up to 4 GiB, and `upload_stored` exists for bodies past the 16 MiB buffered cap. (2) A 39 MiB RESTORE answers 200 with 218 of 400 objects written; the other 182 items carry 500. (3) 200 PUTs of 128 KiB in about 1 s write 170; the rest answer 500, at ack=quorum and ack=none (202) alike. The counts match the cap: 170 × ~97.5 KiB (zstd of the document) and 218 × ~76 KiB are each about 16 MiB. The answer is 500 "consensus call rejected": `propose_entry` maps only `closed` (shard_group.cc:742), so the reason is lost. No acknowledged write is lost. The shard takes writes again after the next autocommit (60b5b8) and after a `kill -9` at the cap (d069b2). Master has no such bound. CI does not catch it: `coverage_e2e_server.py`'s 18 MiB upload runs in no CI job or ctest. | jobs 261124-190201-acf64e (L), 261124-190906-60b5b8 (restore, burst); consensus core.h:1036-1040; shard.cc:363, :424; site changelog.md:28-31 | Checkpoint on log pressure, independent of `modified`. For example: when a placement leaves the retained log past half of `log_bytes`, schedule a checkpoint on the group's loop (never from a commit hook), and have a `LogFull` refusal await it and retry once. Answer a refusal that remains with 503 naming the reason. State the per-shard bound in journal-durability.md. Test (fails at this commit, per acf64e and 60b5b8): on a quiet 1-shard index, a 40 MiB streamed upload reads back byte-equal; a 39 MiB RESTORE writes 400 of 400; 200 PUTs of 128 KiB in one autocommit window all succeed, at both levels. |
| J1 | yes | defect | **One client write can stop the node from ever starting again.** Index paths resolve inside the journal host's directory (round 1's N4, now run). `PUT /.xapiand/journal/groups-abc-000000001/x` answers 204 and creates a directory named like the journal's table files; `l…-turns-…` does the same with a turn log's name. At the next start the journal's scan takes the directory for its own file, and the node exits 74: "The journal in .xapiand/journal cannot be recovered: open existing journal file: Is a directory". Every index on the node is then unavailable until an operator finds and removes that directory, and the natural operator response, moving the journal aside, leads into J2. | jobs 261124-192149-c90be0, 261124-191056-51e8cd; journal host.h:165-170 (`shard_of`), :839-846 (`scan`) | Refuse every index path under `.xapiand/journal` before anything is created. Better: refuse everything under `.xapiand/` except the system indexes the node makes itself. Or move the journal's directory out of the namespace index paths resolve into. Test: that PUT answers 4xx, creates nothing, and the node restarts (fails at this commit, per c90be0). |
| J2 | yes | defect | **An index whose group the journal does not hold opens as a new group over its old state.** The trigger is a journal lost or moved aside after a clean stop, the response J1's refusal invites. strata creates a fresh group, and adopt of an empty bundle makes the file set's recover delete every `app.*` file: the index's committed blob volume is gone. The next six writes, and creating a new index, answer 500 "The index did not apply the operation": strata skips entries at or below its head. Worse, a refused blob PUT still runs its file effects at the fresh state's `vol-1` offset 0, where the old document's locator points. GET of the old document then answers 200 with the other document's bytes: silent corruption. Locators carry no checksum (shard_group.cc:182-201). | job 261124-191523-ffc17f (lost); strata group.hh:262, :1152, :1183; file_set.hh:118-141; shard_group.cc:205-236 | Never create a group over an index that holds state. When the host has no group and strata's applied head is past 0, or `app.*` files exist, refuse the open with an error naming the missing journal group, and delete nothing. For example, strata's adopt fences on `!restored && head_ > 0`, beside its P_b > P_H check; the lead routes the strata half. Or Xapiand checks before the group opens. Test: blob document, COMMIT, clean stop, journal moved aside, restart. The index must refuse naming the cause, `app.vol-1` keep its bytes, and a later blob PUT leave the document's bytes unchanged (fails at this commit, per ffc17f). |
| D1 | yes | defect | **A damaged file set aborts the node at the index's first request.** Here `app.vol-1` was truncated below its bundle length after a clean stop. strata opens the group damaged and arms its repair timer (group.hh:279). Xapiand's catch-up then throws `replica_damaged` (shard.cc:265). `open_blocking` rethrows without closing the opened group, and the catalog destroys it on its own thread. strata allows that only for a group never opened, closed, or failed to open (group.hh:222-230). `~group` cancels the timer off the loop: "Assertion failed: (runs_here() && "blocking_scheduler::cancel off the driving thread")" in an ASSERTS build. Each restart that touches the index aborts again. No line names the damage before the abort. Without the abort, the answer would be strata's "the replica is damaged and reinstalls", and nothing reinstalls a lone voter. | job 261124-191523-ffc17f (damage); src/database/shard.cc:258-280; strata group.hh:222-230, :279, :1629-1631 | When anything after `group.open_blocking()` throws, close the group before rethrowing. A damaged group's close skips its checkpoints and tears down. Log strata's damage reason once, with the index path, and answer it instead of "reinstalls". Test: truncate a volume after a clean stop; two requests on the index each answer an error naming "a file shorter than the bundle names: vol-1"; the log names it; the node stays up in an ASSERTS build (fails at this commit, per ffc17f). |
| N1 | no | nit | Round 1's N1 is addressed as ruling 43 chose: the slice's last command carries the counter. A crash between a slice's batches is an unanswered RESTORE whose ids can be past the logged counter, so one later POST may answer 409. | shard.cc:1118-1124, :1219-1235 | None needed. |
| N2 | no | nit | Unchanged from round 1: an upload's final seals its volume without checking its length against the record's size. | shard_group.cc:307-309 | Compare the lengths before the seal; a mismatch is a corrupt apply. |
| N3 | no | gap | Unchanged from round 1: `commit_blocking` clears `modified` (shard.cc:474), so a later COMMIT returns without a checkpoint or a log cut. L1's fix makes this matter less. The owner's call: keep, or track the group's published position instead. | shard.cc:317-343, :363, :424 | Owner's call; at least document it. |
| N5 | no | nit | journal-durability.md contradicts itself: item 6 says a request's commit awaits the group's checkpoint (:125), item 7 that it stays a plain strata commit (:131). | docs/mechanisms/journal-durability.md:125, :131 | Drop "and a request's commit" from :131. |
| N6 | no | nit | `PUT /.xapiand/journal/layout/x` answers 500 "filesystem error: in create_directories: Not a directory", not a 4xx. J1's fix covers it. | job 261124-192149-c90be0 | J1's fix. |
| N7 | no | nit | A stored blob's GET reads its whole record into one string (shard_group.cc:182-201), so a 4 GiB upload's GET holds 4 GiB. Master's `storage_get_stored` did the same, so this is not weaker. | shard_group.cc:182-201 | Stream reads when uploads reach those sizes; not this quest. |
| N8 | no | gap | Once D1 is fixed, one damaged volume makes the whole index refuse at rf = 1; master served everything but that volume's blobs. Whether a lone voter fails closed this way is the owner's call. Options: (a) keep failing closed and state it, with the operator's recovery path (DUMP/RESTORE, then remove the marker); (b) serve documents and refuse only the damaged volume's blobs. | strata group.hh:1168-1185 (adopt_damaged) | Owner's call; state it in journal-durability.md either way. |

## Not run

- A power cut of a whole node at Xapiand's level, journal, index and file set together, in either mode. The quest
  defers it (`power_loss_check.sh`, `--later`). strata's own `lone_voter_test` cuts power around a group, but with
  its `blob_app`, not Xapiand's application. Probe S shows only which calls are made.
- Linux and FreeBSD: probe S ran on macOS; completion's `sync_call` makes the same calls in both modes there.
- ENOSPC on the journal's volume or on the file set; a sealed volume failing its CRC32C (the same damaged open as
  D1); a 32 GiB rotation (the bounded seal is 7adbab's test).
- A release build (ASSERTS off) of D1's path. By the code, the destroyed group detaches from the loop with its Driver
  row still open, so the index would fail later opens with some other error. Not run.
- after_probe.py's index-copy case (60b5b8 "moved"): invalid as written, because a copied index also needs the node's
  system index (404 "No such database: p_moved/.__2"). The journal-aside case in ffc17f replaces it.
- None of the contributor's evidence jobs was rerun.
- I am a subagent of the lead's session, not a session of my own, so no `xapiand · reviewer journal-replaces-wal
  durability` title applies.
