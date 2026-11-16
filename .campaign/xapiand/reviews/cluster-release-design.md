# cluster-release review: design

Commit: Xapiand:5dc60256ac392a991bf12dee492c39ad83bdcafe
Verdict: CHANGES
Date: 2026-11-15

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right: one strata group per shard at every `rf` (no WAL at `rf = 1`), one root group,
document mode with prepared commands, one transport with a request lane, idle groups closed.
Six gaps would be built in wrong or could not be proven: the install drops stored blobs (F1),
RESTORE and T11 have no defined path or baseline (F2), the shipped read levels cannot give the
linearizable histories the gate asks for (F3), wildcard index patterns walk only the local disk
(F4), the command size bound names the wrong mechanism (F5), and the root's cost at millions of
indexes is unstated (F6). Each fix is a decision and a criterion in the note, not a new approach.

## Evidence I relied on

No jobs: the note is a design and nothing is built or measured. I relied on the code and documents
it cites, read at its pins: Xapiand `a9cfa2cba`, strata `9bcb142e`, consensus `a95847e`, journal
`8f3e410`, Detent `13a2ae42` and `5e4903e8` (the note on Xapiand's driver needs), Detent's
`docs/design/async-drivers.md`, strata `docs/REPLICATION.md`, `docs/DISTRIBUTION.md`,
`docs/design/track-c.md` and `docs/design/durability-modes.md`. Section 1's Xapian-era citations
are history and I took them as stated.

## What I checked myself

- `.scratch/review-cluster-release-design/evidence.sh` (read-only, seconds, no hold): prints every
  code and document line the findings cite, at the pinned commits; its output is `evidence.out` beside
  it. Rerun after a revision to recheck the facts each fix answers.
- Held up under the named risks: consensus admits a membership change only after an entry of the
  leader's own term commits, one voter or witness at a time (consensus `ARCHITECTURE.md:31`), so
  D7's replace steps are safe; reopening a closed group as its old leader keeps one leader per term,
  because quiescence requires every member matched (track-c D12) and a deposed leader at an old
  term commits nothing; failure detection is per node pair (track-c D12), the transport per host
  (async-drivers D10), the journal per core with one barrier per turn, and an idle group holds no
  descriptor under D97 Q3 (b); an `rf = 1` group whose node is down cannot be replaced by D7's steps
  (no leader to add a learner), so it waits rather than splitting; the sortable-encoding note is
  right that no code ordering is needed (D87).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A replacement or a new replica loses `stored` blobs. D12 writes each voter's `DataStorage` at apply, but quest 6's install moves only strata's segment pack from a pinned manifest, and the journal is truncated at checkpoints. `DataStorage` volumes are Xapiand files, located by a node-local volume counter and offset, and no install carries them. A learner from D7's replace, quest 9's `rf` growth or quest 16's migration holds documents whose locators name volumes it lacks. After the dead voter is removed and a second replacement follows, the blobs live on one copy, then on none: acknowledged content lost through repairs that each stay inside the fault model | design :223-227, :336-340; `src/database/shard.cc:1100-1130`; strata `REPLICATION.md:88-100` ("Strata owns its segment files ... the journal never copies them") | (a) The checkpoint bundle names `DataStorage`'s state at the applied position (each volume and its length, synced before the manifest), and install moves it as an application file set on the same transfer (durability-modes D17's file sets; rotated volumes are immutable, so a retry sends only what is missing). Say where a partly streamed upload's volume stands at a checkpoint. (b) Blobs move into strata (track-d D7, today a non-goal; owner's call). Recommend (a). Test: after a replace, `rf` 1 to 3 and a migration, every stored blob reads back byte-equal from the new replica, and a mutant that skips the volumes fails; quest 16's digest covers blob bytes |
| F2 | yes | gap | RESTORE has no defined path, and T11 has no baseline. RESTORE opens shards with `DB_DISABLE_WAL`, and `profile_bulk.sh`, the procedure quest 10 A3 names for T11, profiles a RESTORE. The WAL writes no record there (the profile's WAL share was the per-checkin `delete_wal`), so "the journal at most half of the WAL per document" has nothing to halve. Two paths are possible, and the note picks neither. Through the journal: T12 (RESTORE docs/s at least 1.0x pre-7c) and R8's ingest absorb a log write and a barrier per turn that master never paid. Around it: a second durability path at `rf = 1`, with nothing for replicas | `src/server/search_views.cc:1687`; `src/database/handler.cc:1159`; `harness/profile_bulk.sh:22-31`, `:76`; `docs/perf/bulk-index-profile.md:49`; strata `REPLICATION.md` Xapiand mapping ("bulk-import command whose segments the leader builds") | Decide RESTORE's path at `rf = 1` and `rf > 1`. Give T11 a baseline where the WAL writes: PUT or bulk with the WAL on, at its async default and at `DB_SYNCHRONOUS_WAL`. Put T12's RESTORE row in quest 10's criteria on the chosen path, numbers stated even if the journal loses |
| F3 | yes | defect | The shipped read levels cannot give the linearizable get-by-id histories the gate requires. D4 ships `read=leader`, which durability-modes D10's one vocabulary does not have (linearizable, bounded, session, prefix, eventual), and section 4 lists `leader` among the levels that follow the release. Undefined, `leader` is the node that believes it leads, and consensus has no leases. A deposed leader, partitioned or reopened at its old term, serves stale reads until CheckQuorum steps it down (up to `election_max`, 2 s by default). R2, D14 and quest 7 still require linearizable get-by-id histories, which neither `prefix` nor an unprobed `leader` provides. D1's "failover never produces two leaders" holds only per term, as R3 states it | design :41, :147-156, :240, :257, :343, :425-426; strata `durability-modes.md:2047-2056`; track-c D7 (CheckQuorum), timing profile | Ship `read=linearizable` at `quorum` as `?volatile`'s successor: Core's quorum-probe read, already a Driver call (async-drivers D2, D8), plus the live version map for get-by-id. Or define `leader` in D10's table with its staleness bound. State the read level of each R2 history, and restate D1 as R3 does. Test: the checker flags a planted stale get-by-id from a partitioned old leader at `prefix` and `leader`, and passes at `linearizable` |
| F4 | yes | gap | Wildcard index patterns miss every index with no local shard. Patterns expand by walking the local data directory, and D10 fans out only named indexes. In a cluster, a pattern search (nested paths, `twitter/*`) silently returns partial results, with nothing in `_shards.failed` | `src/server/search_views.cc:2342-2362` (`index_pattern::expand(Endpoint::cwd, ...)`) | Resolve patterns from the authority's namespace records. D5 keys them by path, so a pattern is a range scan with nested paths included: the cluster form of `name-index`, one mechanism and not a second index. Quest 14 criterion: a wildcard search from any node of 3 and 5 nodes returns the same indexes and hits as the one-node walk over the same tree, nested paths included |
| F5 | yes | defect | The command size bound names the wrong mechanism. D12 bounds a command at 64 MiB, "the journal's batch bound". Consensus's per-group defaults cap a command at 1 MiB (`command_bytes`), an RPC at 2 MiB and the retained log at 64 MiB, and an entry above `Flow::frame_bytes` goes alone. A 64 MiB append holds every other group's control frames on the shared node-pair connection for about 0.5 s at 1 Gb/s, longer on slower links, past the 1 s minimum election timeout: the `reader_test` hazard. Master accepts a 1 MiB non-stored blob and stored uploads to about 4 GiB, and a serialized document with its terms is larger than its JSON | consensus `include/consensus/types.h:55-67`, `ARCHITECTURE.md:41`, `:216`; `src/database/handler.cc:94`; `src/database/pool.h:161`; async-drivers D10 (writer never behind more than one append frame) | Name consensus's limits as the mechanism. Set Xapiand's per-group `command_bytes`, `rpc_bytes`, `frame_bytes` and `log_bytes` from the largest document master accepts (limits rule). Carry every blob above the frame cap as chunk commands (D12's stream path). Criterion: a maximal document and a 4 GiB upload commit while another group on the same connection keeps its heartbeat gaps under the minimum election timeout |
| F6 | yes | gap | The root group's cost at millions of indexes is unstated. D5 puts a namespace record per index and a shard record per shard into one group of 3 to 5 voters, and every index creation (indexes are created on first write) becomes a strict root proposal before the shard group exists. R10 states the authority as functions of the node count, and section 10 calls it fine "at the release's scale" in nodes. The questions that decide quest 8's bytes are open: records in memory with whole checkpoints, or a strata index on the root's core; bytes per index; the root's checkpoint and install time; index creations per second; and whether each node's shard-map cache holds every record or fills on a miss | design :158-168, :433, :521-522; memory `many-indexes-workload` (millions of indexes per host) | State these in D5 with arithmetic. Quest 8 criterion at 10^6 namespaces: root memory, checkpoint and install time, creation rate and latency, a node's cache size. R10 states the same as functions of the index and shard counts |
| F7 | no | defect | "Answers equal one node's" (D10) holds only per replica: ranking statistics count dead ordinals, and the physical segments of document-mode replicas differ, so one query ranks differently by replica, and quest 14 A1 holds only on corpora without deletes | strata `durability-modes.md:395-397`; `REPLICATION.md:84-86` | Take a shard's statistics and partial from one replica per request, and state the residue. A1 adds a corpus with replaces: logical results (counts, aggregations) exact, ranking compared within one replica choice |
| F8 | no | gap | Raising `number_of_replicas` from 0 to 1 (`rf` 1 to 2 + witness) passes through a voter plus a witness, or two voters, and neither is an allowed mix; quest 9 A3 tests only `rf` 1 to 3 to 1 | track-c D9 (`track-c.md:389-397`) | State the path. Test 0, 1, 2, 1, 0 |
| F9 | no | gap | "Runs short" (D7) is undefined when there are fewer nodes than voters (default `number_of_replicas` 1 on two nodes, `rf` 3 on two): a two-voter group, which REPLICATION.md does not support, or a voter plus learners | design :180-182; `REPLICATION.md` replica count | Say which |
| F10 | no | defect | File sets and edges. Quest 10 lists `src/database/{shard,wal,handler}.*`, but the WAL also lives in `src/database/pool.*`, `src/manager.*`, `src/opts.*` and `src/xapiand_storage_io.h`, overlapping quest 12, which can run beside it. Quests 14 and 15 run together and both write `src/server/search_views.cc`, and 15's `docs/**` holds 16's `upgrade.md`. Missing edges: 3 and 12 after `detent:consensus-transport` (the node keepalive and node authentication); 10 and 13 need 1; 15 needs 9 | `git grep` at `a9cfa2cba` (evidence.sh F10); design :276-294, :405-410; Detent:5e4903e8 ("after consensus-driver and consensus-transport") | Correct the file sets and edges |
| F11 | no | defect | Quest 13 A1 needs section 1's whole checklist on 5 processes, but items 3 and 4 (search and aggregations from any node) depend on quest 14's fan-out | design :72-78, :365 | Move items 3 and 4 to quest 14 |
| F12 | no | defect | `format-freeze`'s edges disagree: section 5 says after 3, 4, 5, 8, 10 and 11 "beside" `sortable-encoding`, and section 6 says after `cluster-gate`. Quest 6's install bytes (and F1's file set) and quest 13's records fall outside the first list | design :390, :397, :414-418; `quests/format-freeze.md` ("Lands after sortable-encoding") | After 17 and after `sortable-encoding` |
| F13 | no | gap | Detent plans to create a consensus-quiescence quest itself, and the note lists it as Xapiand's quest 3. Detent's note asks for idle CPU at 1M groups, while A1 counts frames over 10^6 steps | Detent:5e4903e8 | Name one owner before either starts. Align the scale |
| F14 | no | gap | A closed group's records pin turn-log segments until it checkpoints, and a closed group runs no timer, so write-once indexes grow the per-host retained log | strata `REPLICATION.md` (checkpoints by bytes and time); design D11 | Closing checkpoints first, or the reclaimer reopens pinning groups. R10 measures the retained log |
| F15 | no | gap | A lone voter campaigns at `Start` (Detent quest 4), so reopening a closed `rf = 1` group may cost a term bump and its barrier before the first write, on the many-indexes path (D89) | async-drivers D5 (X5) | Say whether a lone voter resumes its stored leadership. R10 measures first-write latency after reopen against master |
| F16 | no | gap | `get` of a stored blob (to 4 GiB) and `forward_write` of a streamed upload need streamed bodies under credit, or a redirect to the owner node, and the lane needs connections to nodes that share no group (D10 opens them only while a group is shared) | design D9, :305-306; async-drivers D10 | Add both to section 8's requests to Detent |
| F17 | no | gap | Membership lives in each group's log, but the shard record also holds members and a leader hint | track-c D1; design :163 | The group log is authoritative. The record follows a committed change and is reconciled after a crash. Leader hints are never written to the root per election. A shard never gets a new group while a member of its old one may live |
| F18 | no | gap | Replay to flush marks covers flush-prefix states, not "each state a real power loss can leave". Replaying every mark of 200 runs of 60 s is hours per run. R8's old cluster needs a multicast L2 | design :241-246, :425 | State the coverage, sample marks (around each ack plus random), estimate the cost, and name `-netdev socket,mcast` |
| F19 | no | gap | Not put to the owner: multicast discovery and `--discovery-*` are removed, `/.nodes` becomes `/:nodes`, `/:cluster/health` and `:drain` are new, and `:flush` (a pathless admin command on master) becomes a per-index checkpoint beside the existing `COMMIT` | design :170-176, :152-153, :185; `src/server/search_views.cc:609`, `:1521-1543` | Add to section 9 with options, including keeping multicast as a seed source and mapping the checkpoint to `COMMIT` (durability-modes Q12) |
| F20 | no | nit | "`search_batched` with one batch per node" conflicts with its contract, which batches whole indexes for their statistics | `src/engine/search.h:147-154` | Quest 14 changes the batch key |
| F21 | no | nit | R8 against the old async replication names no level | design :431 | `none` for like durability, `quorum` beside it |
| F22 | no | nit | D6's keepalive table needs every node connected to the root leader | async-drivers D10 | Say what carries it (the lane or root learners) |
| F23 | no | nit | D8 "forwards at most once" and a redirect on a stale hint: it does not say who follows the redirect | design :189-192, :367 | Say it and count it in quest 13 A2 |
| F24 | no | nit | Quest 6's scrubber goes past parity; reads already fail closed | design :283, :339-340 | Keep or defer, the lead's call |

## Not run

- No build, test, simulator or VM run: nothing exists to run, and my probes are the cited lines
  (evidence.sh).
- Not checked: section 1's Xapian-era citations at `5a517b052^`; prospect's `searcher.hh` claim;
  the segment-mode CPU estimate (0.38x to 0.43x), the 62 ms append figure and R5's comparison
  numbers; Detent's own quest designs beyond what the note depends on; whether master's get-by-id
  reads unrefreshed writes.
- Not read, per the clean room: Xapian's sources, `src/sortable_serialise.{cc,h}`, the Python
  `sortable.py` and strata-oracle.

## Re-check (b7b0a4451)

Commit: Xapiand:b7b0a4451
Verdict: APPROVE
Date: 2026-11-15

Scope: `git diff 5dc60256a..b7b0a4451 -- docs/design/cluster-release.md`, against F1-F24, with
Detent's later inputs (D97 gives `detent:consensus-quiescence` to Detent; consensus-transport's
bulk-budget criterion). Line numbers are the note's at b7b0a4451. Probes:
`.scratch/review-cluster-release-design/recheck-b7b0a4451.sh` (read-only, seconds), its output
`recheck-b7b0a4451.out` beside it, and `evidence.sh` for the old facts.

All six blockers are closed, and the fixes add none. D16 makes volume numbers a function of the
log, names every volume (open uploads included) with its length in the bundle, and truncates and
replays on recovery; chunks that a higher term abandons are deleted in log order on every voter,
so replicas stay equal. D17's T11 baseline writes the WAL, and T12 is measured on the new path and
goes to the owner on a miss. D4's `linearizable` reads at or past the probe's commit index, which
matches strata `REPLICATION.md:134`. D12's limits agree with consensus `ARCHITECTURE.md:41` and
with Detent's bulk-budget criterion, and 13 A5 keeps a control that fails. What remains is
planning and statement work, listed below.

| Old ID | State | Where |
|---|---|---|
| F1 | closed | D16 :368-386; D12 :325-333; quests 5 A1, 6 A1-A2, 10 A7, 13 A6, 16 A1 |
| F2 | closed | D17 :388-401; quest 10 A3, A6 :535-543; R7 |
| F3 | closed | D1 :132-134; D4 :161-183; R2 :637; quests 1 A2, 5 A5, 13 A7 |
| F4 | partly closed (F4 below) | D5 :195-204; quests 8 A5, 14 A4 |
| F5 | closed | D12 :312-333; quests 4 A4, 10 A8, 13 A5 |
| F6 | partly closed (F6 below) | D5 :213-226; quests 8 A4, 9 A4; R10 |
| F7 | closed | D10 :287-291; quest 14 A1 |
| F8 | closed | D7 :250-257; quest 9 A3 |
| F9 | closed | D7 :241-245 |
| F10 | partly closed (F10 below) | section 5 table :441-451 |
| F11 | closed | quests 13 A1 :554, 14 A1 :566 |
| F12 | closed | :592-593, :627 |
| F13 | closed | :118, row 3 :434, quest 3 :486-490 |
| F14 | closed | D11 :303-305; quest 5 A6; R10 |
| F15 | closed | D11 :306-308; section 8 item 3; R10 |
| F16 | closed | D9 :272-275; section 8 items 4-5; quest 4 A4, A5 |
| F17 | closed | D5 :206-211 |
| F18 | closed | D14 :343-363; quest 1 A3 |
| F19 | closed | Q7, Q8 :731-762 |
| F20 | closed | D10 :281-284 |
| F21 | closed | R8 :643 |
| F22 | closed | D6 :230-232; quest 12 A4 |
| F23 | closed | D8 :261-264; quest 13 A2, A3 |
| F24 | closed | section 4 :410 |

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F4 | no | gap | Not every pattern is a prefix scan. `**`, `_all`, `**/some-??`, a leading `*` and `-` exclusions match at any depth, and `.` names are never listed. So `_all` or `**/x` scans all 10^6 records, and the walk's work bound (`walk_gate.h`) has no root equivalent. 8 A5 tests "a prefix scan" only | `src/index_pattern.h:34-46` (recheck); note :195-198, :527-528 | Say that the root scan runs the `Glob` automaton over sorted keys, seeking past subtrees that cannot match, under walk_gate's bound. 8 A5 and 14 A4 cover `**`, `_all`, exclusions and `.` names |
| F6 | no | gap | The rate estimate (:221, at least 1,000 creations/s) includes `rf` × shards group creates per index. 8 A4 measures records only (:524-527), so nothing measures the end-to-end rate. Section 8 calls the namespace record "the only path-to-group map" (:652), yet a node fills its own groups "from their local group metadata" after a restart without the root (:223). At 10^6 indexes per host, the note does not say how a node maps a path to its local groups, or at what memory cost | note :221-223, :524-527, :652 | State that mapping and its cost. Add an end-to-end creation-rate row (with group creates) to R10 or 13 |
| F10 | no | defect | Quest 10's file set still misses WAL users that `land` would refuse to touch: `CMakeLists.txt` (its test list at :1897 names `wal_replay`), `src/database/flags.h`, `tests/test_wal_replay.cc`, `tests/test_{pool,engine_error,reactor_requests,wide_search}.cc`, `harness/wal_{recovery,replay,replay_agg}_check.sh`, and `harness/{README.md,async_allowlist.tsv,async_check.py,removed_options_check.sh,test_gate_tools.py}`. Quest 14 misses `tests/{test_index_pattern,fuzz_index_pattern,fuzz_index_expand}.cc` | recheck F10 (`git grep` at `a9cfa2cba`) | Add them, and give 1 or 10 the harness `run_all.sh` entry for the replaced scripts |
| F25 | no | defect | 12 after 10 is right for the file sets as written. Both claim `src/opts.*` and `src/manager.*`. The mitigation in section 6 (build 12 beside 10 and rebase it) cannot run: `campaign start` refuses overlapping file sets. So the critical path is 5, 10, 12, 13 (2,800 lines, against 1,400 through 6), and the abstract (:52-53) and section 6 (:607-613) name the wrong one | `SKILL.md:27`, `:145`; note :441, :443, :607-613 | Recommend: 12 owns only `src/cluster/{node,membership}*` and its tests, and 13 does the options and manager wiring (13 already needs 10 and 12). Drop 12's edge on 10. Or keep the edge and restate the critical path |
| F26 | no | gap | `log_bytes` is 64 MiB and checkpoints trigger on it, so a 4 GiB upload forces about 64 checkpoints (each a strata commit with its volumes synced). RESTORE checkpoints at a cadence master never paid, which can decide T12. D7's `transfer_leadership` balancing raises the term and, by D12's rule, aborts every open chunked upload in that group | note :239-240, :323, :330-332, :391 | State the checkpoint cadence under RESTORE and uploads, or a larger `log_bytes` for them and what it costs installs. Have balancing skip or wait for groups with an open chunked command. T12's profile shows the checkpoint share |
| F27 | no | gap | The release narrows master's read-your-writes, and the note does not put it to the owner. Master refreshes a dirty shard before every read, so a search or get-by-id on one node sees every prior write. Here the default `read=prefix` reads the last refreshed snapshot with `refresh=false` at 1 s, so on a lone voter a get right after a PUT can miss it for up to 1 s. The owner decides. Options: (a) keep master's behavior where the reader holds the leader's replica (refresh before read when dirty, which a lone voter always is); (b) keep `prefix` as written and list the change in Q8's style; (c) default `refresh=wait_for`. Quest 10 A1's E2E may catch it either way | `src/engine/strata/shard.cc:311-316`, `:1555` (recheck F27); note :164-165, :172 | Add as a question in section 9; recommend (a) |
| F28 | no | nit | D4 cites strata `ARCHITECTURE.md:91-93`, but the file is `docs/ARCHITECTURE.md`, and those lines cover the version check. The source for get-by-id from the version map is `REPLICATION.md:134`. 10 A3 does not say which journal level is compared against which WAL mode (A4 does). D12's chunk must fit `command_bytes` with its header, and "a final entry, the command itself" reads as the whole command, not its header | recheck F28; note :168-169, :325-333, :535-540 | Fix the citation and the wording |

Not checked: whether `DataStorage` writes anything non-deterministic, such as a timestamp in a
volume header (A1's digests and 13 A6 would catch it). Also not checked: Core's quorum-probe read
requiring an entry of the leader's own term (consensus's own tests). Per the clean room, I did not
read Xapian's sources, `src/sortable_serialise.*`, `sortable.py` or strata-oracle.
