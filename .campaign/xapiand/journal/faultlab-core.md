# faultlab-core: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: steps 1-6 done; remaining A1/A4 mutant jobs queued.

## Next

1. Lead: create github.com/Kronuz/faultlab and push, for A6's CI half (no remote here); add the
   cpp-libraries AGENTS.md catalog entry; launch the correctness review.

Worktree: /Users/kronuz/code/Kronuz/faultlab-wt/xapiand/faultlab-core (faultlab repo, branch quest/xapiand/faultlab-core).
Done (faultlab commits): skeleton bfce6a8; faultfs d9d420b, 4923aa4, 4d9f951; recorder e607680;
scenario/events 854eda0; Python half 58880e6; README, docs, demo a5dcc10. Local: Release ctest
15/15, ASan+UBSan C++ green (before the Python half), ruff and pyrefly strict clean.
Mutants are local refs mutant/faultlab-core/<name> (.scratch/mutant.sh); each fails its test locally.
Older jobs: build-test 261118-003124-8f88ab (at 4923aa4), 261118-005459-889c2a (at 58880e6).

## Blocked on

A6's CI half: the faultlab repo has no GitHub remote, so CI cannot run until the lead creates
Kronuz/faultlab and pushes (`.github/workflows/ci.yml` calls the shared cmake.yml with
`freebsd_packages: python312`). Recommendation: create it at landing; land with `--no-ci` only if
the owner wants it before the repo exists.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261118-010005-2aea3a; fails without: 261118-005949-37fe70 (additions), 261118-005953-3d76e7 (forced-skips-draw) |
| A2 | 261118-010005-2aea3a; fails without: 261118-003136-4b6402 (stream-shift) |
| A3 | 261118-010005-2aea3a; fails without: 261118-003131-9c42c3 (no-sector-cut) |
| A4 | 261118-010005-2aea3a; fails without: 261118-003126-596e36 (link-orphan), 261118-005958-a9f786 (fold-mutates) |
| A5 | 261118-010005-2aea3a; fails without: 261118-005504-2f7a32 (recorder-bytes), 261118-005516-67f979 (proxy-close-swallows), 261118-005529-db2445 (selftest monotonic, seal) |
| A6 | [artifact] faultlab:a5dcc10 README.md, docs/api.md, docs/mechanisms/crash-model.md, examples/demo.cc (run by CTest). CI: open, see Blocked on. |

## Result

New repo faultlab (branch quest/xapiand/faultlab-core, 8 commits):

- include/faultlab/faultfs.h: strata's faultfs ported onto completion's journal::IO with
  directories, link, preallocate, map_shared, try_lease, inspection, owner locks, kill, snapshot,
  powered_off and crash_policy (A1, A3, A4). All 384 recorded strata seeds rebuild the same state
  (test/strata_seeds.inc, A2).
- recorder.h (F10), scenario.h and events.h (F11, F12).
- python/faultlab: history.py with the monotonic and seal checks, fault-aware reports and the replay
  line; proxy.py with F14's Mesh interface; scenario.py (A5).
- CTest runs 15 tests, including the C++ event log read by the Python checker.
- Docs: README.md, docs/api.md, docs/mechanisms/crash-model.md.
- Finding R7: unaligned CrashIO appends reach states no sector pick does.

Open: CI (needs the repo), the cpp-libraries catalog entry, consumer moves (later quests).

## Rulings I made

- R1 link replay: like create, and only while its source still names the file; an op that drops a
  name of a file whose earlier logged link dropped (a remove, or a replace's overwrite) drops too,
  so link-then-unlink never loses the file (the design's "only POSIX-reachable states"). The coin is
  drawn as for any op, so seeds without links are unchanged.
- R2 sync_directory(dir) = settle what the ordering points staged (device-wide, as File::sync
  already did) + make dir's namespace durable. Identical to strata's for one directory (A2 table).
- R3 make_directory is a setup call on filesystem: durable at once, no mutation, no fault rule.
  Removing a directory fails EPERM (POSIX unlink); opening one EISDIR.
- R4 try_lease is no mutation and the power never fails it; a fault rule on op::lease can.
- R5 size_pick::sector draws one rng() where the size coin was (index into the cut list: the lower
  size, sector boundaries strictly between, the higher size). A size pick changes how many sector
  coins follow (strata's loop), so only sector picks leave the later choices to the seed.
- R6 inspect: device 1, owner geteuid(), mode 0600 (directories 0700), extended_acl false.
- R7 A3's CrashIO half uses block appends (storage writes blocks). Finding: with unaligned appends
  CrashIO reaches sizes at write boundaries and byte tears that no faultfs pick reaches (176 of 200
  random sequences in a probe). Real filesystems can leave write-boundary sizes; for the lead/owner
  if a later consumer needs them (size_pick would need a write-boundary cut).
- R8 recorder: event 0 is "start" (the shadow before any call); calls are serialized under one
  mutex; owner.lock creation through the recorder is an event; a shadow that refuses what the real
  IO did throws logic_error ("the shadow diverged"). load() syncs everything; materialize() needs a
  missing or empty directory and makes it 0700, files 0600.
- R9 history.py: `durable: true` on a read marks its node for the monotonic check; reads at level
  `seal` (a new read level) carry numbers and get the seal check, not prefix or linearizable;
  check() keeps returning strings, so Xapiand's checklist moves by path only (step 4).
- R10 proxy.Mesh: heal(a=None, b=None) keeps heal(node); fault events go to any object with
  fault(kind, **args) (history.Recorder has one). Python suites need 3.12 (`type` aliases): CI's
  FreeBSD job installs python312.
