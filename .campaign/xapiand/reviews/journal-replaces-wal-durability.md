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


## Re-check (round 3)

Commit: Xapiand:98ea84a70d623f6370676f9c4525f4e90b9459cd
Verdict: CHANGES
Date: 2026-11-24

Scope: round 2's fixes (Xapiand:f76c395e4, Xapiand:98ea84a70) of L1, J1, J2 and D1; the mechanisms they add (the
pressure trigger and the LogFull retry, the pool's path refusal, the strays move, the open's two refusals); the two
edited probes; the lead's after_probe question; and the lead's async_check question. The branch head Xapiand:79106c070
adds only `.campaign/` files. L1 holds under contention and `kill -9`, but its retry still has no test (L1-T). D1
holds. J1 and J2 are fixed for the spellings and the state the round-2 probes used, not at their causes: a
case-folding file system still reaches the journal's directory (J1b), and a journal that holds the group but is older
than the index still corrupts it (S1). CI is red on async_check (A1).

### Evidence I relied on

- 261124-201352-cf165a (ASSERTS on): the four new test_pool cases and test_loop_yield pass. With 261124-201432-1276d7 and
  my f39122 below, it shows D1 at its cause: each open of a truncated volume refuses naming "a file shorter than the
  bundle names: vol-1", a second restart refuses the same, and nothing aborts. The code matches: `refuse_opened_group`
  (shard.cc:325-347) quiesces, closes the group on the catalog's open chore, where strata's close tears down on the loop,
  and only then destroys it; 261124-201409-4408e8 (exit 134, the old abort) fails without it.
- 261124-201426-429bdc (18 and 40 MiB uploads, a 39 MiB RESTORE, the burst), 261124-201427-e8ef49 (wedge, restore 400 of
  400, burst 200 of 200), 261124-201434-2c5992 (`kill -9` with the log at the cap): L1's sequential cases.
- 261124-201429-92f1d4, 261124-201431-758b53: the journal's own names and the two lexical paths answer 400.
- Mutants killed: 07df1f, 0ce20a (J1), 5abd1c (J2), 4408e8 (D1), 01a364 (the RESTORE's status). 16805d survived (L1-T).

### What I checked myself

Code at this commit against strata 7a4284c4, consensus 6e1122cc and journal fc5c59b, and eight probe jobs at this
commit. Scripts are in the worktree's `.scratch/xapiand/review-journal-replaces-wal-durability/r3/`; `job-r3.sh PROBE`
builds xapiand (ASSERTS on) and runs one, and its header has the submit line.

| Job | Script | Result |
|---|---|---|
| 261124-220506-8f8cec, 261124-222934-d94580 | `concurrency_probe.py` | passes: no 5xx; every acknowledged write reads back after every `kill -9` (below) |
| 261124-220841-6c01fb | `restore_probe.py` | passes: 125 MiB RESTOREs into one shard write 1,000 of 1,000 at ack=none (202) and quorum (200); two at once at ack=none, 500 each, write all |
| 261124-220912-e2b871 | `journalcase_probe.py` | fails: J1b (261124-220359-b70147, the first run, failed its restart only on the probe's own node name) |
| 261124-220619-fb40f3 | `stalejournal_probe.py` | fails: S1 |
| 261124-222229-186d66 | `strays_probe.py` | passes (261124-220726-77c60b failed only on the probe's own write, a string into an integer field) |
| 261124-220730-f39122 | `after_probe_r3.py damage` | passes: the refusal names the truncated vol-1 to the client and in the log |

1. **L1.** Twelve writers on one 1-shard index, half at ack=none, beside two streamed 24 MiB uploads, with no COMMIT:
   484 writes, 0 refused, all read back (8f8cec). Then rounds of the same load cut by `kill -9`. In d94580 there
   are 6 rounds, every kill inside the load (each round acknowledged a part of its 1,442 writes); 8f8cec ran 4
   rounds. After each restart every write acknowledged in any round reads back byte-equal, 4,022 at the end, with
   2,019 answered 202 at ack=none and 16 uploads. Writes resume at once. A RESTORE of 8 times the bound at ack=none
   (D113's bulk-loader level), one at quorum and two at once write every object (6c01fb). No 503 appeared in any
   run, so I found no path where the retry gives up while a checkpoint could free the log. The retry itself is
   unproven (L1-T): no test or probe here showed it needed. The bound's rationale is load-bearing: it names its
   mechanisms (replay length; consensus keeps the retained log in memory, core.h `entries_`), it bounds no single
   write (entries are 1 MiB and commands chunk, D12; uploads cross checkpoints, 40 MiB in 429bdc), and its
   comparators are right: Lucene's `DEFAULT_RAM_BUFFER_SIZE_MB` is 16, Tantivy's per-thread minimum is 15 MB, Xapian
   flushes at 10,000 documents (LOG_ENTRIES' 65,536 is above it). None of them refuses a write at its buffer, and the
   probes show the group does not either.
2. **J1.** Every spelling the HTTP layer normalizes is refused (`//`, `/./`, `..`, `%2E`, `%2F`; e2b871). Case is not
   (J1b). The strays move is safe (186d66): plain files it does not own (`.DS_Store`, `notes.txt`) stay and the node
   starts; directories named like table and artifact files, a symlink to a directory outside and a name already taken
   in the strays directory (suffixed `.1`) move, and no journal file does; a `kill -9` with 2 of 20,000 strays moved
   leaves a node whose next start moves all 20,000 exactly once and serves its documents.
3. **J2 and D1.** The lost journal refuses and touches no `app.` file (1276d7; its refusal comes from the system index
   `.xapiand/indexes/.__1`, so cf165a's unit test is what shows a user index's own refusal). A journal put back from
   an older copy is not refused (S1). D1 as above.
4. **The two probe edits.** Both fair, so I did not rerun the originals. 758b53: the original criterion counted every
   new file in the journal, and the allowed `PUT /.xapiand/x` (index `.xapiand`) adds the journal's own table files
   (its log lists 5 `groups-*`); the edit keeps what J1 needed (both journal paths 400, no index file inside the
   journal, the restart). Neither version lists directories; e2b871 does. 1276d7: J2's own fix text asked for a
   refusal that names the missing group and keeps the bytes, and the edit accepts exactly that, adding the blob PUT's
   refusal and every app file byte-equal.
5. **The lead's after_probe question.** "damage" was the probe's assumption: the first volume is `app.vol-1` at both
   commits (60b5b8 at 0ea6bd04a printed it too). With the glob fixed, f39122 passes. "moved" fails cleanly: the fresh
   node has no settings for the copied index, takes it for 5 shards, and answers 404 "No such database:
   p_moved/.__2"; its writes go to new shards nested in the copy, the copy's three strata files are byte-equal to the
   source's after the run (sha256), and the node stops cleanly. The owner's decision stands.
6. **async_check (A1).** Thread by thread, the hits CI reports:
   - shard_group.cc:565-576, the strays move and the second `create_directories`: ShardGroups' constructor has one
     caller, `DatabasePool::set_journal`, and that has one, the manager's constructor (manager.cc:301). It runs on
     the main thread before the loops and listeners exist, and before this object's own group loop (:579-580).
     Off-loop. pool.cc's `comparable_path` in `set_journal` runs there too.
   - pool.cc's `inside` and `comparable_path` in `resolve`: the catalog's open chore on a WorkPool worker (`h.exec`,
     the context of rows 14-15). Off-loop; path arithmetic and one getcwd.
   - shard_group.cc:787 and :802-804, `pressure_mtx_` in `pressure()` and in its task's completion: **on the group
     loop**. `placed` runs there (strata group.hh:299-304, after propose_entry moved onto the loop). Every holder's
     section is O(1) over two flags with no I/O; quiesce() holds it only for its check, and `cv.wait` releases it. So
     the loop never waits behind a long section under write pressure.
   - shard_group.cc:812-814, quiesce()'s `pressure_cv_.wait`: close_group (`close_payload`) and refuse_opened_group
     (`open_payload`) run in the catalog's chores on WorkPool workers; `~ShardGroup` runs wherever the catalog
     destroys the payload. On the group loop it would deadlock, since the pressure task it waits for needs that loop,
     so the row needs a check, not only an argument.

### Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| J1b | yes | defect | **The journal's directory is still reachable under another spelling, on a file system that folds case.** The pool compares an index path with the journal's by name (`inside`: lexical and case-sensitive). APFS, macOS's default and this host's, folds case. So `PUT /.Xapiand/journal/x`, `/.xapiand/Journal/sub1/x`, `/.XAPIAND/JOURNAL/sub2/x` and a 2-shard `PUT /.Xapiand/Journal/sub10/` each answer 204 and make index directories (`.__1` to `.__5`, `sub1`, `sub2`, `sub10`) inside `.xapiand/journal`. While the node runs, four directories named like another index's next artifacts (`g<group>-artifact-<incarnation><serial+1..4>`, made the same way, 204 each) fence that index. Its next COMMIT answers 500 "storage fenced outside worker", and every write after it 500 "strata::group: the replica fenced", until a restart. At the restart the strays move takes the client indexes' directories to `.xapiand/journal.strays`, so their acknowledged documents answer 404 "No such database". | job 261124-220912-e2b871; pool.cc:551-570, :617 | Compare by identity, not by name. In resolve, before anything is created, refuse when the index path or any existing ancestor of it is `std::filesystem::equivalent` to the journal's directory: a few stat calls in the open chore, off the loops. Test, failing at this commit on every platform: with an alias of the journal's directory (a symlink `alias -> .xapiand/journal` under the data root), pinning `alias/x` answers `invalid` and creates nothing. On a case-folding volume, `PUT /.Xapiand/journal/x` answers 400 (rerun e2b871). |
| S1 | yes | defect | **J2's class, at its cause: a journal that holds the group but is older than the index corrupts the index.** `refuse_without_journal` refuses only when the journal holds no group (`holds`: stored, closed or bounded). Putting back a copy of the journal taken at an earlier clean stop passes that check, and J2's own refusal text advises exactly that ("restore the journal"). strata's adopt fences only P_b > P_H. So the file set's recover truncates `app.vol-1` to the old bundle's lengths, and the group's new log positions fall at or below strata's head. What the probe saw: GET b2 (acknowledged and committed) answers 500. j4 at quorum and b3 answer 500 "The index did not apply the operation". j5 at ack=none answers 202 and is never applied, so it is 404 after a COMMIT and after a restart: an acknowledged write lost. b3's file effects run anyway, so GET b2 then answers 200 with b3's bytes: silent corruption, J2's symptom. `app.vol-1` and the manifests changed on disk. A copy of a running data directory (journal copied before the indexes) and a plain-mode power cut on macOS reach the same state. | job 261124-220619-fb40f3; shard.cc:298-318; strata group.hh:1181 | Fence the cause. An index whose strata head P_H is past the end of the group's recovered log (its last durable index, or a closed group's base) is ahead of its journal: refuse before the file set's recover touches anything, naming both positions. An entry is durable before it applies, so a sound pair always has P_H at or below that end, and the check never refuses one. The check needs the recovered log, so it sits in strata's open between `driver.recover` and `adopt`; the lead routes the strata half. Xapiand can pre-check a closed group by comparing its closed record's base with `index.applied()`. State in journal-durability.md ("Refusals at open") that only the journal as of the index's last stop may be put back. Test: the probe's sequence as a test_pool case. The open refuses naming the cause, every file of the index stays byte-equal, and nothing is applied (fails at this commit, per fb40f3). |
| A1 | yes | gap | **CI is red at this commit: async_check finds this round's blocking calls unclassified** (run 45545297347, Build + unit + smoke). Ruled thread by thread in item 6: the strays move, `comparable_path` and `inside` run off the loops (the manager's constructor; the catalog's open chore); `pressure_mtx_`'s sections run on the group loop but are O(1) on every side; quiesce()'s `cv.wait` runs off the loops on every current caller, and on the group loop it would deadlock. | CI run 45545297347; harness/async_allowlist.tsv rows 14-15, 66, 68-69; src/engine/sync.cc:48-68 | Widen rows 66 and 14-15 (count and note) with the callers above. Add an allow row for `pressure_mtx_` like row 68 ("an O(1) section on each side, no I/O"), naming the group loop. Make quiesce() checked: call `engine::check_may_block("ShardGroup::quiesce")` first, which aborts in ASSERTS builds on a reactor or a task scheduler's thread, then give it an allow row that cites the check. Confirmation: CI's `async_check --strict` green and test_pool/test_node green with the check in place. |
| L1-T | yes | gap | **The LogFull retry has no test that fails without it.** Mutant 16805d sets `LOG_FULL_ATTEMPTS = 1`, which drops the retry. Its message says the trigger goes too, but the diff keeps the trigger. It passes `test_a_log_past_its_bound_checkpoints_instead_of_refusing`: the sequential test never meets LogFull, because the trigger alone keeps it under the bound. None of my probes showed the retry needed either. The lead says a contributor is strengthening the test. | job 261124-201402-16805d (one line, shard_group.cc:836) | A case that meets LogFull, for example with the pressure checkpoint held or the log near the bound at open (2c5992's state), killing 16805d. Also a mutant that drops the trigger, killed by the same test. |
| N9 | no | nit | Overlapping checkpoints subtract the pressure counters twice. `checkpoint()` loads `log_bytes`/`log_entries` before `group.checkpoint()`, which waits its turn behind a running checkpoint (strata group.hh:347). The second checkpoint then subtracts bytes the first already subtracted. The counter undercounts (the trigger fires late) or wraps (`size_t`: it fires at every placement until the next checkpoint). This happens whenever a writer's LogFull checkpoint or a COMMIT overlaps the pressure one. It costs liveness only, and LogFull's retry covers it. | shard_group.cc:748-752, :852-856 | Subtract exactly: keep (index, bytes) per placement and drop those at or below `published.through`. |
| N10 | no | nit | The counters start at 0 at every open, while the recovered log may be near the bound (a `kill -9` at the cap). The first writes after such a start then go through LogFull, not the trigger. It works (2c5992). | shard_group.h (`log_bytes{0}`) | Checkpoint once after catch-up when the replayed log passes half the bound, or seed the counters from it. |
| N11 | no | nit | Every consensus refusal now answers 503, TooLarge and DuplicateRequest included. Those mean a proposer bug (entries are chunked to 1 MiB), and a 503 tells the client to retry forever. | shard_group.cc:864-869 | 500 for TooLarge and DuplicateRequest; 503 for LogFull, Busy, NotLeader and NotReady. |
| N12 | no | nit | The strays move renames into a sibling, `<journal>.strays`. With `.xapiand/journal` a symlink or mount on another volume, a common place for a log, any stray makes `rename` fail with EXDEV, and the start ends EX_IOERR. | shard_group.cc:566-576 | Name EXDEV in the start's error, or keep the strays directory on the journal's own volume. Not run. |

### Not run

- No sanitizer build: the questions did not need one, and CI runs ASan and TSan.
- A runtime-damaged group (damage met by an apply or a read while open) through close and a restart. The open path is
  D1's, run above.
- A power cut at Xapiand's level, in either mode (as in round 2). S1 is what a plain-mode cut on macOS can leave.
- Linux and FreeBSD: every probe ran on macOS (APFS, case-folding). J1b needs a case-folding volume; S1 and the
  rest do not.
- N12's EXDEV, and Unicode-normalization spellings of the journal's path (its name is ASCII, so case is the only
  folding that applies).
- I am a subagent of the lead's session, not a session of my own, so no `xapiand · reviewer journal-replaces-wal
  durability` title applies.
