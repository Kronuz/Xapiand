# schema-load-once review: correctness

Commit: Xapiand:7993e7aa2c26
Verdict: CHANGES
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test 261113-002138-fae0f9 at 7993e7aa2: the suite passes (A3).
- Mutant 261112-232218-983be8 (at 2142c7da0; schemas_lru, single_flight and the tests are the same at
  7993e7aa2): with the probes off, the three paused-claim tests fail on the load count. The probe half
  of the fix is tested.
- Paused-claim tests 261112-224510-9fae0e and replicas test 261112-225104-1e0203; I re-traced the
  replicas outcome by reading the code (risk 3 below) and did not rerun it.
- ASan x20 (261113-002610-d39bb8) is still queued; I do not rely on it.

## What I checked myself

Probe job 261113-004029-efd733 (exit 0), from `.scratch/review-schema-load-once-correctness/`
(`probe.sh`, `patch.py`). It adds a test-only seam `after_metadata_flight`, awaited in `_update` right
after `metadata_loads.run` returns, and the test
`test_review_publish_serves_a_claim_between_flight_end_and_store`. Call B misses the cache and is
held before its claim. Leader A loads, its flight ends, and A is held before its own cache insert.
B is then released: it claims a new flight and probes.

| Build | seam test | three paused-claim tests |
|---|---|---|
| 7993e7aa2 as is | ok, 2 loads | ok |
| the three loads' publications removed (`load_metadata`, `load_foreign`, `load_and_publish`'s `store_settings`) | FAIL, 3 loads (test_node.cc:2505) | ok, all three |

Answers to the five risks, by reading 7993e7aa2:

1. **Publish before end.** Each store is a single section under the cache lock, so no caller ever
   sees half an entry. Nothing that can throw follows a store: after `schemas[...] = ...` and
   `store_settings` there is only `co_return`. An error reaches every joiner: `run` rethrows
   `flight->error`, and joiners loop only on `Abandoned` or `task::cancelled`. The schema cache uses an
   optimistic compare (`schema == local_schema_ptr`) on every write. A published entry that a writer
   then replaces is handled the same way master handles its insert-in-continuation race. No code
   erases a schema from the cache except LRU aging, so a delete is unchanged.
2. **Probe.** A probe hit is the same as a cache hit at the top of `_update` or
   `resolve_index_settings` by a call that arrived a moment later, and it is a locked snapshot. One
   difference from master: a probe hit on a cached fallback initial foreign schema returns
   `failure=false`, where master's reload would set `failure=true`. This changes nothing. `get()`
   ignores `failure`, and `set()` always has `new_schema`, so it never reaches the read branch
   (schemas_lru.cc, the `else if (new_schema)` branch).
3. **Settings.** The flight returns S0: default shards and replicas, placed, `loaded=false`. The
   paths:
   - A `number_of_shards` change sets `rebuild`, then `shards.clear()`, then
     `calculate_shards(N)` and `settle_replicas`. This is master's computation.
   - A replica-only change sets `rebuild`, then `settle_replicas` runs from S0's nodes. A shrink keeps
     the first R+1 nodes. A growth continues the same walk after the primary. Both give master's exact
     placement for the same `Node::nodes()`, so dropping the clear at 7993e7aa2 is correct.
   - A strict or invalid miss throws before the save. The later outcome of each kind of call matches
     master's (N5).
   - A concurrent caller with different settings applies them to its own copy and saves. A version
     conflict reloads with `loaded=true` (`save_settings` sets it), and a shard-count change then gets
     the same ClientError as on master.
   - A probe hit on a saved entry also has `loaded=true`, which gives the same result.
4. **Extracted functions.** The bodies of `initialize_loaded`, `place_shards` and `store_settings`
   match the inline code they replace. The returned `shard_settings` equals the stored one. `lk` is
   unlocked on both branches before `store_settings`, so there is no self-deadlock. The second
   `initialize_loaded` on the flight path sees placed shards and never sets `rebuild`:
   `settle_replicas` caps replicas at `total_nodes ≥ 1`, and `load_settings` throws on empty replica
   lists.
5. **Allowlist.** The new sections are: the stores in `load_metadata` and `load_foreign` (the guard is
   a body local, released at `co_return` before `final_suspend`); `cached_metadata`; `cached_foreign`
   (two sequential sections, with the MsgPack copy outside the lock); the `read.cached` re-read;
   `store_settings`; and `cached_settings`. None awaits, none does I/O, and none nests with
   `SingleFlight::mtx_`. The claims hold (N6).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| G1 | yes | gap | Publish-before-end, half of the fix, has no failing test. The paused-claim tests wait until the other five calls finish (`done == N-1`) before releasing the paused one. By then each finished call's own continuation has stored the schema or settings, so the probe hits with or without the flights' publication. The publication alone closes the remaining window: a claim that lands after the flight ended and before any caller stored. That window is the CI flake's interleaving under slower scheduling. | Probe 261113-004029-efd733: with the three publications removed, all three paused-claim tests pass and the seam test fails with 3 loads. | Add the seam test from `.scratch/review-schema-load-once-correctness/patch.py`: one seam after the flight returns, as in the probe, or the same shape per flight. Tag a `--fails-without` job whose mutant removes the publications (`patch.py mutant`); it must exit 0 only on that test's load-count CHECK. Settings and foreign variants are optional; one flight shows the mechanism. |
| N1 | no | gap | A failed load publishes nothing, so for errors a second load can still start in the window between the flight's end and the first caller's fallback insert. This needs a foreign load that throws: `doc_not_found` when the index's settings document was never saved, for example a non-writable first use of an index that does not exist. In the quest's test the foreign document exists (`endpoints_of` saves the settings), so it does not apply there. The window is narrower than master's. | schemas_lru.cc `load_foreign`, and the catch blocks after `foreign_loads.run` | Keep as is (errors are not cached by design), or state the limit in single_flight.h's comment. |
| N2 | no | nit | `cached_metadata` ignores `versions` (outdated), while `cached_foreign` checks it. `SchemasLRU::updated` has no callers, so nothing changes today. | schemas_lru.cc `cached_metadata` vs `cached_foreign` | Check `versions` in both for one shape, or drop the check from both. |
| N3 | no | nit | The two-argument `SingleFlight::run` no longer has a caller. | single_flight.h `run(Key, Load)`; grep of src and tests | Delete it; the one way is `run(key, load, probe)`. |
| N4 | no | nit | The flight caches `.__1..__opts.num_shards` shard entries. A caller asking fewer shards leaves the extra entries until they age out. Nothing resolves a shard path (`resolve_index_endpoints` passes the unsharded path, and `invalidate_settings` has no caller), and master reaches the same state after a non-writable first read. | index_resolver_lru.cc `store_settings`, `resolve_index_endpoints` | None needed. |
| N5 | no | nit | A strict or invalid writable miss now leaves the flight's unsaved default placement in the cache, where master left nothing. Later calls get master's outcome: a non-writable call gets the same default placement, a writable call re-applies its settings, and a strict call throws again. | index_resolver_lru.cc `load_and_publish` | None needed. |
| N6 | no | nit | `cached_settings` copies the IndexSettings under the lock, O(shards × replicas). This is the same bound as the existing store section, so the allowlist's "O(log n)" note is loose but the claim of no I/O and no await holds. | index_resolver_lru.cc `cached_settings`; harness/async_allowlist.tsv | Optionally say "O(shards)" in the note. |

## Not run

- TSan, and the ASan x20 loop (A2; queued as 261113-002610-d39bb8).
- Settings and foreign variants of the seam test. G1's probe covers the metadata flight only, and the
  mutant removed all three publications at once.
- Multi-node behavior. Placement equality with master is argued from `settle_replicas`'s walk for one
  `Node::nodes()` snapshot, not run against a changing node list.
