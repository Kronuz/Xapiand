# turn-log-parity review: design (durability lens), strata docs/design/rocksdb-parity.md, round 2

Commit: strata:e5721fd681b380fb4cc7a8b5f83275e55980ce61
Verdict: CHANGES
Date: 2026-11-20

The revision closes the first round's F1, F2, F4 and N1-N7 as text; F7 stays the owner's. The new
risk is F6's answer. As written, D4's checkpoint loses acknowledged writes (model, F8), and its
background install drops two orderings that `commit()` keeps today (F9, F10). The park text
predates `journal:idle-park` and conflicts with it in three places: a second state, forward
copies removed under held idle groups (F11), and D6's pull against loop-owned table shards (F12).
D1's ladder breaks the arithmetic journal uses to size every turn (F13). Read-your-writes, the
tolerant open and large transactions (A1-A3) have no mechanism text in this note or in
`standalone-turn-log.md` (F14). Every blocking row is a text fix plus the test it names. D148's
shape stands.

## Evidence I relied on

- Probe F (261120-083804-ec1239), probe G (261120-083208-36162a, `all ok`; its N5 rows read in the
  log) and probe B2 (261120-083211-0cbbce): all exit 0, and the note's tables cite them. The
  first round's probes B, D and E, as that review accepted them.
- A4's cohort test (261120-082445-11a61b, and its mutant 261120-082448-f562c7), taken as the
  status shows them. Not rerun.
- Code read: strata e5721fd6 (`index.hh`, `turn_seal.hh`); journal cc95ba6 (strata's pin),
  9e11713 and 1222e3b (`main`); Detent's `journal-idle-groups` at journal:c3fb805
  (`docs/design/idle-groups.md`) and `docs/mechanisms/host-ranges.md` at 1222e3b; RocksDB 11.11
  (`~/.scratch/rocksdb-11.11.fb`) and `bench/engines/rocksdb-11.11/kv_rocksdb.cc`.

## What I checked myself

Scripts are in `.scratch/xapiand/review-turn-log-parity-design/round2/`. Rerun them with
`python3 -I <script> <depth>`.

- `d4_covers_model.py`, job 261120-124240-07f24d (exit 0, depth 11). It explores every event
  sequence of one logged group (join, two-chunk transaction, turn, freeze, install plus
  checkpoint, close) with a power cut after each prefix. It then checks that every transaction
  whose record is durable is in the reopened index.

  | covers rule | install waits for the log | close needs covers = seq − 1 | states | silent loss | torn tx | log_lost | exposed past V |
  |---|---|---|---|---|---|---|---|
  | `note` (D4 item 3) | yes | yes | 89,864 | 5,443 | 1,196 | 0 | 0 |
  | freeze closes the record | yes | yes | 76,253 | 0 | 0 | 0 | 0 |
  | covers one back when shared | yes | yes | 89,733 | 0 | 0 | 0 | 0 |
  | freeze closes the record | yes | no | 76,579 | 319 | 0 | 0 | 0 |
  | covers one back when shared | yes | no | 90,248 | 503 | 0 | 0 | 0 |
  | covers one back when shared | no | yes | 127,641 | 0 | 0 | 18,902 | 18,902 |

  The first sequences that fail are:
  - `JFJTIT`, loss: a write joins the record after the freeze, and the checkpoint covering that
    record drops it.
  - `JFCTJIT`, torn: a transaction's first chunk shares X_f's record.
  - `JTFJITL`, close loss: the last record is a checkpoint that covers less than every batch.
  - `JFI`: an install before the log is durable.
- `d4_install_order_model.py`, job 261120-124622-b44851 (exit 0, depth 12). Two frozen buffers
  are in flight, with checkpoints in freeze order as the note says. With installs also in freeze
  order, 0 of 18,059 states lose anything. If the newer build installs first, 1,916 of 21,427
  lose acknowledged writes. The first failing sequence is `JFJTFO`: the head passes the older
  buffer, and replay skips its transactions.

## Answers to the lead's questions

1. **Durability.**
   - **D4 as written loses acknowledged writes.** D4 item 3 covers X_f.index, but that record
     takes more transactions after the freeze. `freeze()` leaves the open record open
     (`index.hh:5056-5061`), and `log_state::join` appends to it (`:492-511`). The losses are
     silent (F8).
   - **The background install skips commit()'s orderings.** It must wait for the log through
     X_f.index (`index.hh:1258-1259`, `:3545-3548`), and builds must install in freeze order
     (F9).
   - **Close must cover every batch.** After task 6, a close or park whose checkpoint covers less
     than every batch drops records. The drop is silent: `close_group` checks only the sequence
     (journal 9e11713 `host.h:419-425`), and the reopen accepts head = X (`index.hh:3352-3356`)
     (F10).
   - **Park and reopen.** Detent's design is sound and covered by its own crash table. This
     note's park text contradicts it (F11). The D6 pull model can seal a woken group's first
     record before its loop commits the wake. Recovery then reads the stale `closed` entry and
     drops that record silently (F12).
   - **Torn records.** These are unchanged: the incomplete tail turn is dropped whole. A large
     transaction spanning seals is torn by F8's rule (1,196 states). It also needs commit and
     abort rules, which no design states (F14).
2. **Read-your-writes.**
   - **Nothing is designed.** The mechanism has no text (F14).
   - **What the note touches.** Readers at V must never see a frozen buffer's writes through an
     installed segment (F9's first half; 18,902 exposed states without the wait). A merge changes
     no visibility.
   - **D11 under background flush.** It can resurrect an old copy past a concurrent merge (N12,
     only if Q4 is taken).
3. **Large transactions.**
   - **Memory.** D7's per-log counter bounds the unsealed chunk bytes to one room. The
     transaction and its buffer form are the rest. D4's half-share active limit may cap A3's
     "unlogged limit" (F14).
   - **Turn seal contracts at journal:9e11713.**
     - **Repeat sealer calls.** journal `main` already calls the sealer again and resumes after a
       drained group, so task 5's journal half is done (N8).
     - **Retired floor at the close's sequence + 1.** It is consistent with attach's restage at
       `max(base, retired) + 1` (`index.hh:3357-3362`).
     - **Create and reopen durable at the first turn.** strata's join tolerates a crash between
       its head edit and the create's turn, because `status none` joins again
       (`index.hh:3337-3340`, `:3415-3434`). D6 does not (F12). `turn_seal.hh:110` ("each is
       durable when it returns") stops holding when the pin moves (N8).
   - **Stall.** Each chunk adds up to two turns for every other writer on the log, or on the
     device under D6. The note does not state this (F14). D1's smallest segment sets the chunk
     rate (F13).
4. **RocksDB parity.**
   - **Claims that hold:**
     - D7: RocksDB's unsynced WAL window has no time bound.
     - D3: RocksDB builds a `WriteBatch` and copies it into the writer's buffer.
     - D4's two-buffer bound: `max_write_buffer_number` defaults to 2 and kv_rocksdb leaves it.
     - D9's trivial move.
     - D11's blind `Put`.
   - **Claims that do not hold:**
     - D1's "one empty file" holds on macOS only (N10).
     - D5's stated cause for RocksDB's grouping does not fit a 21 ms barrier (N11).
     - D9 omits RocksDB's per-level binary search (N13).
5. **Buildability.**
   - **Order and tests.** Tasks 1-8 build in order. Each has a named test once F8-F10 and F13 are
     in the text.
   - **Duplicated scope.** Task 9's "durable `parked` state", and task 13's streaming and
     creation in the turn, are `journal-idle-groups` D5-D6 (F11).
   - **Missing steps.** No task moves strata's journal pin (cc95ba6) to `main` (N8). The quest
     file gives tasks 3, 5 and 9 three items: the copy flag, a per-fragment checksum, and the
     `block_bytes` refusal. The note's task rows list none of them. The quest file carries them,
     so I raise no separate row.

## Prior findings

| ID | State at e5721fd6 |
|---|---|
| F1 | Closed (D6's handoff, probe F), except the table transitions (F12) |
| F2 | Closed (D6's damage scope, the shared-log A2 test, the roster as task 14) |
| F3 | Superseded by `journal:idle-park` (Detent D1: parked is closed); the note's version conflicts with it (F11) |
| F4 | Closed (per-log bound, probe G) |
| F5 | Closed through task 12; task 13 reopens it (F11) |
| F6 | Answered, but the answer has the defects F8-F10 |
| F7 | The owner's (Q4) |
| N1-N7 | Closed. N3's text counts joins; the merged cohort wait counts released groups (`turn_seal.hh:334-341`), and the quest lists N3 as a follow-up. N5's rule is journal `main`'s already (N8). |

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F8 | yes | defect | D4 item 3's `covers` = X_f.index supersedes the record that holds X_f. That record keeps taking transactions after the freeze until a turn seals it, because `freeze()` leaves the open record open and the checkpoint closes it only when staged. A power cut after the checkpoint's barrier loses those writes silently. A two-chunk transaction whose first chunk shares that record comes back torn. | Job 261120-124240-07f24d: 5,443 of 89,864 states lose and 1,196 are torn (`JFJTIT`, `JFCTJIT`). Both fixes lose 0. `index.hh:492-511`, `:5056-5061`. Note lines 204-213. | Option 1: `freeze()` closes the group's open record, so X_f ends its record. Option 2: `covers` = X_f.index − 1 when that record holds a later position. Replay already skips positions at or below the head (`index.hh:995`). Test (task 6): a write joins the open record between the freeze and that record's seal, power is cut after the checkpoint's barrier, and the write recovers. Also, a two-chunk transaction across that record recovers whole. The mutant `covers` = X_f.index fails both. |
| F9 | yes | gap | D4 moves the flush off `commit()` without two of its orderings. (a) `commit()` waits for the log through the applied position before it flushes and commits (I1, "no flush publishes above V"). A background install that does not wait shows readers at V the frozen buffer's crash-removable writes through the segment (A1's "another reader does not"). A power cut then leaves the head past the log (`log_lost`). (b) The note orders checkpoints in freeze order but not installs. A newer build that installs first moves the head past an older buffer whose transactions replay then skips. | Job 261120-124240-07f24d: 18,902 states exposed past V and 18,902 `log_lost` without the wait. Job 261120-124622-b44851: 1,916 of 21,427 lose with out-of-order installs, 0 in order. `index.hh:1256-1273`, `:3545-3548`, `:3373`. | D4 item 2 states two rules. First, an install's `commit_files` and its publish wait until the log is durable through X_f.index. Second, builds install in freeze order, or only one builds at a time. Test (task 6): a reader at V during a build with `ack=none` writes in the frozen buffer sees none of them before their barrier. A second test runs two builds with the newer finishing first and cuts power after its install; every acknowledged write recovers. |
| F10 | yes | gap | After task 6, a group's last record can be a checkpoint that covers less than every batch. `leave()` hands such a checkpoint to the table whenever it is the last record. `close_group` checks only that the sequence is not behind the last record, then supersedes everything through base. The reopen accepts the table's record when the head equals its X. Batches after `covers` are then lost silently. `journal:idle-park` D1 assumes "a checkpoint covering every batch" too. | Job 261120-124240-07f24d: 319 and 503 states lose (`JTFJITL`) when a close takes a partial checkpoint, 0 when the close requires `covers` = sequence − 1. `index.hh:3437-3445`, `:3352-3356`. journal 9e11713 `host.h:419-425`. | D4 states that `leave()`, and so close and park, drains: no build in flight, and a final checkpoint whose `covers` is its sequence − 1. Task 6's journal change makes `close_group` refuse a checkpoint with `covers` < sequence − 1, which makes any miss loud. Test: close with batches after the newest checkpoint's `covers`; the close drains or refuses, and the reopen holds every write. |
| F11 | yes | defect | D10 item 3 and task 9 define a second park: `GroupState` 4, a table format change. The quest file rules it out ("consumes it and defines no second park"), and Detent's D1 rejects exactly this ("a new `parked` state beside `closed`"), along with the first round's version. Task 13 then removes `forward_slice`. But `journal:idle-park` parks only released groups (D2: "parking a held group is refused by construction"; strata parks by closing). It keeps forward copies as the fallback for held idle groups (D8). Every open strata index holds its group, so once task 13 lands an idle open index's checkpoint pins its segment, and the log grows until ENOSPC fences every index on it. That is F5 again. Task 13's streamed re-partition and creation in the turn are idle-groups D5-D6. | Note lines 136-143, 595-614, 700, 704. journal:c3fb805 `docs/design/idle-groups.md` D1, D2, D8, "Assumptions about turn-log-parity". `turn_log.h` `reclaim()` frees only segments with no live record. | D10 item 3 consumes `journal:idle-park`: parked is closed, and strata parks an idle index by closing it through `leave()`, after F10's drain. Tasks 9 and 13 drop the parked state, the streaming and creation in the turn. Task 13 removes `forward_slice` only if strata asks every quiet group for a fresh checkpoint before its record leaves the window, checkpoint-only groups included. Otherwise it keeps `forward_slice`. Test (task 13): an open idle index plus 10 × `retain_segments` of another group's turns, with `forward_slice` removed; the segment count stays at or below `retain_segments + spare_segments + 1`. |
| F12 | yes (before task 10) | gap | D6's handoff pulls every record on the owner's thread. `journal:idle-park` makes create and wake durable through a `before_turn` hook that commits dirty table shards before the turn. Each shard belongs to its loop's thread, and no lock guards it. Detent's design assumes that under one log per device "each loop commits its own dirty slots before it hands its sealed records to the owner's turn". A pull cannot guarantee that. If the owner seals a woken group's first record before its loop commits the wake, a power cut leaves that record durable under a `closed` entry, and recovery drops it silently (idle-groups D5's lost alternative). `Host::park(loop)` also walks "the loop's log index" (D10), which under D6 belongs to the owner. | Note lines 365-395. idle-groups.md D5, D10, "Assumptions about turn-log-parity". host-ranges.md:19-23 at 1222e3b. | D6's protocol names the order. Option 1: a group's sealer returns nothing until the group's transition is durable, and the loop's commit wakes the owner. Option 2: the owner commits every loop's dirty shards in `before_turn` under a table lock, with its cost stated. Either way, state which thread `Host::park` runs on under D6. Test (task 10): a wake on a non-owner loop with a power cut at every operation; no acknowledged record of the woken group recovers under a `closed` entry. |
| F13 | yes (before task 3) | gap | Journal sizes every turn by `options.segment_bytes`: `TurnLog::fits`, the encode's "turn exceeds a segment" `length_error`, and `TurnDriver`'s whole and half room. D7's counter is `turn_room(segment_bytes / 2)`. With D1's ladder below `segment_bytes`, a turn sized by that room may not fit the segment it opens. The encode then throws, the driver fails, and every index on the log fences in normal operation. Admitted `ack=none` records also stop sealing within one turn. At the ladder's bottom the room is one chunk per turn, the cap D7 removes for `wal` p99.99. | journal 1222e3b `turn_log.h:407`, `:477-480`. 9e11713 `turn_driver.h:496-498`, `:547-548`. Note lines 85-99, 434-443. | D1 states that the room (seal_turn's whole and half, `whole_room`, D7's counter) follows the segment the next turn writes, or that a turn opens a segment at least its size. A step down never leaves admitted bytes above the next room. Test (task 3): from an empty log, K groups saturate `ack=none`; no turn is refused and every record seals within 2 turns of its join. Rerun `wal` p99.99 from a fresh log. |
| F14 | yes (before tasks `ryw`, `tolerant-open`, `large-tx`) | gap | A1-A3's mechanisms are designed nowhere. This note touches them in two lines (D1 item 1, D6's damage scope), and `standalone-turn-log.md` Q3-Q5 lists options only. Each needs text, because each depends on this note. Large transactions: a chunk joins without moving `applied` (`join` sets it today). Chunks are logged only once the transaction is accepted, or an abort record ends a refused one. Replay applies a transaction only with its last chunk, and a missing middle chunk is `log_lost`. F8's `covers` never passes an unapplied chunk. Chunks take from D7's counter: a T-byte transaction holds the log for ⌈T / room⌉ turns and adds up to 2 turns per chunk to every other writer on it. The text states the memory bound and whether D4's half-share active limit lowers the largest transaction. Read-your-writes: the writer reads at applied across the active and frozen buffers, and others read at V (F9a). Under D6, V comes from the owner's thread. Tolerant open: on the shared log, "covered" means the head is at or past every live record lost, not that the newest record is a checkpoint (task 6). | Note lines 88-89, 402-404. standalone-turn-log.md:508-535. `index.hh:492-511`. Job 261120-124240-07f24d torn rows. | Write the three mechanisms, here or in `standalone-turn-log.md`, with these constraints and each criterion's test, before those tasks start. |
| N8 | no | nit | D7 and probe G describe journal at cc95ba6. Journal `main` already calls a sealer again while it returns records, and resumes after a drained group (9e11713 `turn_driver.h:562-614`). No task moves strata's pin. The move brings contract changes strata must meet: repeat calls replace `turn_records = 1`, the retired floor sits at close + 1, the close can end in doubt, and create and reopen become durable at the first turn once idle-groups lands (`turn_seal.hh:110`). | journal `git log cc95ba6..1222e3b` | Rebase D7's text on `main`. Add the pin move as the first journal task, with the suite run. |
| N9 | no | gap | D2 item 2 divides (`retain_segments` − 1) × `segment_bytes` among the groups that wrote. At 10k writing groups and 64 MiB segments, each group checkpoints about every 19.7 KiB, and every checkpoint is an index commit with its own syncs. | Note lines 133-135 | Give the trigger a floor, and measure it on the many-indexes workload before task 4. |
| N10 | no | nit | "As RocksDB's WAL costs one empty file" holds on macOS only. On Linux RocksDB preallocates each WAL with `fallocate(FALLOC_FL_KEEP_SIZE)` in blocks of 1.1 × `write_buffer_size`, about 35 MiB at the bench's 32 MiB memtable. The comparison still favors strata. | RocksDB `db_impl_write.cc:3428-3440`, `env/io_posix.cc` (`ROCKSDB_FALLOCATE_PRESENT`) | Say "on macOS", and state the Linux case. |
| N11 | no | nit | D5's stated cause of RocksDB's grouping ("followers spin and yield before sleeping") does not fit a 21 ms barrier. `AwaitState` spins 200 times and yields for at most `write_thread_max_yield_usec` (100 µs) before blocking, so its followers sleep through the barrier as strata's writers do. The measured rows stand. | `write_thread.cc:76`, `:144-189`; `options.h:1475` | Drop the cause, or measure it. |
| N12 | no | gap | Under D4, D11 resolves unversioned upserts in the background build and sets delete bits on segments that a concurrent merge may replace before the install. The old copy then survives in the merge output. | Note lines 515-522 | If Q4 is taken, resolve at install under the lease or remap through the merge. Test: an upsert during a merge of its key's segment. |
| N13 | no | gap | D9's parity with trivial move covers writes. RocksDB also finds a key's file in a level by binary search, while the note's gets walk segments by key range, and D9 leaves the count of disjoint segments unbounded. | Note lines 503-510 | State the range index that gets use, and a cap on the segment count. |
| N14 | no | gap | D4's freeze keeps `apply_pending()`. When the active buffer holds pending deletes, that call waits on the writer's thread for the log through the applied position, so on logged overwrite rows every freeze still costs the writer a barrier. | `index.hh:5056-5066`, `:3545-3548` | Move the pending deletes to the install, after F9's log wait, and say so in D4. |

## Not run

- No build or suite. Tasks 2-14 have no code at e5721fd6. The cohort wait's A4 jobs were taken
  as recorded.
- Detent's idle-groups code (journal:b55e34d and later) was not read. Only its design doc was.
- Not modeled: D6's handoff on Linux (task 10's floor), the roster (task 14, reasoned only), D11,
  and D9's merge-runs branch, which is still in progress.
- No RocksDB run. The parity rows rest on the gate's and the probes' numbers.
- The quest file's journal follow-ups (copy flag, per-fragment checksum, `block_bytes` refusal)
  were not reviewed. They are Detent's designs.
