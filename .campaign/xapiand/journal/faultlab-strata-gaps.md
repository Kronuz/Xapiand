# faultlab-strata-gaps: journal

State: waiting on jobs CI run 44879138800 (A6, GitHub Actions at faultlab:b5640d7); every campaign job is green
Quest: .campaign/xapiand/quests/faultlab-strata-gaps.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing beyond CI: the code, tests, docs and campaign jobs are done at faultlab:b5640d7 (pushed).

## Next

1. When CI run 44879138800 is green on every platform: State "ready for review" (correctness).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-231216-b15c65 (faultfs_test: power_on, operations, sync_latency cases) |
| A2 | 261120-231216-b15c65 (model_test: strata's 16 cases) |
| A3 | 261120-231216-b15c65; fails without: 261120-231240-34dc25 (sync mutant, faultlab:7d38b8d), 261120-231241-145756 (reclaim mutant, faultlab:e25e663) |
| A4 | 261120-231216-b15c65 (seeds_test: the F3 table, unchanged, passes) |
| A5 | `docs/api.md`: "Calls of one kind so far, failed ones included" (rows `power_on()`, `operations(op)`, `sync_latency(d)` at b5640d7) |
| A6 | CI run 44879138800 at faultlab:b5640d7 (running) |

The mutants are local branches `mutant/faultlab-strata-gaps-sync` and
`mutant/faultlab-strata-gaps-reclaim` in the faultlab repository (not pushed), each b5640d7 with
one A3 fix reverted. At the sync mutant only "a sync failing after its effect ..." fails (view
`a`, not `b`); at the reclaim mutant only "reclaim-candidate opens ..." fails (ENOENT thrown).

### A1: every faultfs member strata calls (strata main, test/ and bench/)

| strata | faultlab | Same meaning, or the difference |
|---|---|---|
| `filesystem(capacity)` | `filesystem(capacity)` | same |
| `fault{kind, nth, error, after, keep}`, `add_fault`, `clear_faults` | same | same |
| `op::{write, sync, sync_directory, create, replace, remove, truncate}` | same values, same order | faultlab adds `link`, `preallocate`, `lease` after them |
| `media`, `add_media_fault(name, ...)`, `media_hits()` | `add_media_fault(path, ...)` | same; a root name is its path |
| `classify`, `set_trust`, `map_files`, `short_writes`, `damage_writes`, `damaged_writes`, `set_capacity` | same | same |
| `power_off_after(n)` | same | same; faultlab also folds shared mappings first (strata has none) |
| `power_on()` | **added** `power_on()` | process crash at the cut: kill()'s release (handles stale; locks, leases, mappings released) plus the power back; strata's only resets the power, and its two callers drop every handle first, so they see the same state |
| `mutations()`, `syncs()`, `calls()` | same | same |
| `operations(op)` | **added** `operations(op)` | same; `op::lease` counts try_lease calls (strata has no leases) |
| `sync_latency(d)` | **added** `sync_latency(d)` | same: each File::sync (sync_data through it) sleeps `d` before its effect, outside the lock |
| `crash(seed)` | `crash(seed, crash_policy = {})` | the default policy is strata's stream (F3 table) |
| `view(name)`, `list()`, `used()` | `view(path)`, `list(directory = {})`, `used()` | same at the root (sorted names; strata has no directories or preallocation) |
| `io(fs)` | `io(fs, directory = {})` | same at the root |
| `acquire_owner` busy: `EAGAIN` | `EWOULDBLOCK` | one value on Linux, macOS, FreeBSD (`static_assert` in model_test) |
| `open_reclaim_candidate_read_only(missing)`: throws ENOENT | returns null | A3: io.h's contract |
| `open_reclaim_candidate(missing)` (io.h default: open_existing, ENOENT) | returns null | A3 |
| File::sync failing after its effect on a staged file written after the point: the staged snapshot | the flushed bytes | A3 |
| `STRATA_FAULTFS_MAP` | `FAULTLAB_MAP` | F2 |

### A2: strata's test/faultfs_test.cc (strata main) to faultlab's test/model_test.cc

Each of strata's 16 blocks is one `run(...)` in model_test.cc, in strata's order, with its seeds,
loops and checks unchanged (CHECK_ERRNO for `error_of(...) ==`):

| strata block (its comment) | model_test.cc case |
|---|---|
| Synced data and a synced directory survive every crash | "synced data and a synced directory survive every crash" |
| Unsynced bytes: per sector, old or new | "unsynced bytes: per sector old or new, ..." |
| Unsynced creates may vanish | "unsynced creates may vanish" |
| A rename is atomic | "a rename is atomic: ..." |
| Faults before and after their effect | "faults before and after their effect; ..." |
| A failed sync with `keep` | "a failed sync with keep: ..." |
| Capacity and short writes | "capacity and short writes" |
| Stale handles and the owner lock | "stale handles and the owner lock" |
| The directory lock | "the directory lock: ..." |
| An ordering point | "an ordering point: ..." |
| The barrier's second phase | "the barrier's second phase: ..." |
| The same when the log's sync fails after its effect | "the second phase when the log's sync fails after its effect" |
| A write to a staged file after the ordering point (CB-5) | "a write to a staged file after its point is durable only by the file's own sync" |
| A staged file written after its ordering point | "a write after the point never survives without the staged tail before it" |
| Ordering points chain | "ordering points chain: ..." |
| A write after an ordering point to a file it did not stage | "a write after the point to a file it did not stage ..." |

## Result

faultlab:b5640d7 adds `power_on()`, `operations(op)` and `sync_latency(d)` to faultfs, settles the
two A3 behaviors, and ports strata's faultfs_test as test/model_test.cc. Docs: faultlab
`docs/api.md` (the members and both behaviors), a README highlight for `power_on`. Nothing
measured: no performance claim. Leaves open: strata's adoption (faultlab-adopt-strata); the
Xapiand hunt rows at hunt.md:100-101 are settled in faultlab and close when strata adopts it.

## Rulings I made

- `power_on()` releases as `kill()` does, then restores the power: strata's comment says "a process
  crash ends at the cut", and faultlab's process crash is `kill()`; with live handles or mappings
  a bare reset would let stores made after the cut land at the next fold. Changing `kill()` to
  restore the power would change an existing member (A4, contract).
- `operations(op::lease)` counts try_lease calls, so every op kind has a count; leases stay out of
  `mutations()`, as before.
- `sync_latency` waits only in File::sync (and sync_data through it), as strata's does; an
  ordering point's stage and sync_directory do not wait. snapshot() and the move carry the latency
  and the per-kind counts with the rest of the configuration.
- The two A3 fixes change existing behavior, as the quest requires. journal and consensus pin
  faultlab:a3b71d1; no caller relies on either old behavior (a reclaim open's null is io.h's
  contract; no seed scenario syncs a staged file with an after fault, and the F3 table passes).
- Reclaim-candidate opens return null for a directory too: io.h's "null means absent or
  unsafe/nonregular".
