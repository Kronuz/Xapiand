# name-index review: design

Commit: xapiand:1f78c77321f939224ece4aa1afb24a8fd8a4b551
Verdict: APPROVE
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- The accepted correctness review (`name-index-correctness.md`) and its F1 and F2 fixes at xapiand:74faa0d. The names
  answer as the walk does, and never refuse a pattern the walk answers. I did not redo its oracle, TSan stress or
  EMFILE probe, and I carry F3 to F6 from it.
- A2's measurement, job 261117-110258-76c693 (strict, macOS x86_64, warm): a build takes 0.62 s, 6.20 s and 167.5 s at
  10^4, 10^5 and 10^6 indexes; a lookup takes 2.1 µs; a name with 8 bytes takes 16 B. Rerunning A2 at this commit is
  the lead's job.

## What I checked myself

1. Q1, the model. The decision that is expensive to reverse here is "nothing persisted", and it is the cheap
   direction: no bytes on disk, no format and no recovery. Lazy against eager, a snapshot hint, or feeding the names
   from strata's catalog can each be added later without a migration. D1's case against a persisted table holds: it
   would be a second truth, it would need an intent record synced on every create, and it would still miss indexes
   copied in by hand. strata keeps no list of the partition's indexes to borrow, since a cold index is only its
   directory. The cost of this model is the window after a restart (D1 below).
2. Q2, deletes and renames. Read in `name_index.{h,cc}`: the base is immutable between merges, the delta is a
   `std::set`, and `build()` swaps a fresh base under the exclusive lock and drops the delta entries that the walk
   found (`name_index.cc:265-272`). Tombstones fit that shape: the delta becomes name to present or erased, applied
   at a merge and at the build's swap. No delete or rename path exists today (`search_views.cc:1514` answers 501),
   so nothing can make the names stale from inside the node. D2 below names what D9 still has to say.
3. A stale name, which is the only delete a user has today (removing a directory by hand), costs no correct answer.
   A search skips a shard whose index is missing (`src/database/handler.cc:1489-1496`), and it opens with `DB_OPEN`,
   so it never recreates the directory. D9's "a search over it skips it as it skips a missing plain name" is true.
   I queued a node probe for this (`.scratch/review-name-index-design/stale.py`, job 261117-182519-07b8c8). The
   code settled it before the job left the queue, so I cancelled the job; the script is kept to rerun.
4. Q3, the boundary. There is one `NameIndex` per `DatabasePool`, declared before `catalog_`, so it outlives the
   resolves that feed it (`pool.h:458`). Its scope is the local data directory, the same as the walk's. Shards
   `.__N` collapse to their index name (D3), so local shards need nothing more. A pattern across the cluster needs
   the shard map, which neither the walk nor this structure is. If strata's catalog later owns a list of the
   partition's indexes, the feed is one line in resolve (`pool.cc:570`) and the lookup's signature mirrors
   `index_pattern::expand`, so moving costs little. The boundary is right for now.
5. Memory. A name costs its length plus 8 B in the base (D4). With names of 24 to 32 bytes that is 32 to 40 MB at
   10^6. While the build runs, `found` (a `std::vector<std::string>`, 32 B plus any heap per name) and `fresh` are
   both alive (`name_index.cc:240-264`), so the peak is about 3 times the steady state once. This is acceptable.
6. Host, to read A2's curve: this Mac's `kern.maxvnodes` is 263,168 (`sysctl`).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | no | gap | After every restart, a node over the cap answers no wildcard for the whole build. The window opens at the first pattern, not at boot. At 10^6 top-level indexes a walk reads 10^6 root entries, which is past the default `--max-search-indexes` of 100,000, so until the build ends (167.5 s in A2) every pattern answers 400, `w00*` included. With a lazy start, the first pattern after a restart always falls in that window. The build is also superlinear: 62 µs per index at 10^5 and 167 µs at 10^6. A2's tree has 2×10^6 directories, and this host's vnode cache holds 263,168, so the 10^6 build runs with a cold cache. A cold restart costs about the same anywhere, and Linux is not measured. | A2 job 261117-110258-76c693; `sysctl kern.maxvnodes`; `search_views.cc:2356-2363`; `docs/src/content/docs/options.md:294`. | Owner's call; each option is reversible: (a) keep lazy (ruling R2) and say in D5 and the options doc that wildcards over the cap answer 400 until the build ends; (b) start the build at boot as the same `merge` chore, at the cost of one worker and one full walk on every start; (c) write a snapshot of the names at a clean shutdown and use it as a hint while the verifying build runs, with D9's staleness until the build ends. Recommendation: (a) now, plus a Linux cold-cache run of A2 at 10^6 before the release decision. |
| D2 | no | gap | D9's delete path is still one sentence ("calls `NameIndex::erased(name)` after removing the directory"), which F3 showed is not enough, and it misses shards. `twitter/.__1` and `twitter/.__2` each add `twitter`, so removing one shard must keep the name while another shard or `strata` remains. An `erased(key)` must therefore re-derive the name from the disk (`holds_index` on the parent, before the lock, because there is no I/O under it). It must be ordered against a create of the same key by the catalog's per-key order. It also needs F3's tombstone at the build's swap, and it must keep `twitter/user` when `twitter` goes. A rename is not defined. Per index (only the data directories move) it is an erase plus an `opened()`; renaming a whole subtree is a range operation. | `docs/design/name-index.md` D9; `name_index.h` has no `erased`; `pool.cc:570`. | Write these in D9 for the delete quest: tombstones in the delta, re-derivation from the disk per key, the catalog's per-key order, and renames per index, not per subtree. The test, when delete lands: erase a shard of a sharded index, erase during a build, and erase `twitter` while it holds `twitter/user`, each compared with the walk. |
| D3 | no | nit | `Base::ends` is `std::uint64_t`. Offsets fit in `std::uint32_t` below 4 GiB of name bytes (about 10^8 names of 40 bytes), so a 32-bit offset would save 4 B per name, 25% of A2's 16 B. Front coding is already deferred (D4). | `name_index.h` `Base`. | When front coding is decided, or keep 64 bits and say so in the Limits. |
| D4 | no | gap | Carried from the correctness review's F4, and it weighs more at 10^6: a node that never takes a pattern keeps every name it has opened in the `std::set` delta, at about 64 B plus the name each, and never merges. | `name_index.cc:195`. | As F4: state it in D4 and the Limits, or merge before completion. |

## Not run

- The node probe `stale.py` (job 261117-182519-07b8c8, cancelled while still queued). D9's skip rests on reading
  `handler.cc:1489-1496`.
- No measurement of the build on Linux or FreeBSD, or with a cold cache. D1's vnode reading is an inference from the
  curve and this host's cap.
- F5 and F6 (case-insensitive spellings, retrying a build that keeps failing) are not rechecked; they stand as
  written in the correctness review.
