# faultlab-core: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 6: README.md and docs/ (A6).

## Next

1. README.md, docs/ (API, mechanisms), examples/demo.cc.
2. Remaining mutant jobs: additions (A1, policy), forced-skips-draw (A1), fold-mutates, kill-keeps-leases (A4).
3. CI (A6) needs the GitHub repo: the lead creates it at landing; until then CI evidence is open.

Worktree: /Users/kronuz/code/Kronuz/faultlab-wt/xapiand/faultlab-core (faultlab repo, branch quest/xapiand/faultlab-core).
Done (faultlab commits): skeleton bfce6a8; faultfs d9d420b, 4923aa4, 4d9f951; recorder e607680;
scenario/events 854eda0; Python half 58880e6. Local: Release ctest 14/14, ASan+UBSan C++ 5/5 (before
the Python half), ruff and pyrefly strict clean.
Mutants are local refs mutant/faultlab-core/<name> (.scratch/mutant.sh); each fails its test locally.
Jobs: build-test 261118-003124-8f88ab (A1-A4, at 4923aa4, exit 0); link-orphan 261118-003126-596e36 (A4),
no-sector-cut 261118-003131-9c42c3 (A3), stream-shift 261118-003136-4b6402 (A2), all failed as required;
build-test 261118-005459-889c2a (A1-A5, head 58880e6); recorder-bytes 261118-005504-2f7a32 (A5),
proxy-close-swallows 261118-005516-67f979 (A5), selftest monotonic+seal mutants 261118-005529-db2445 (A5).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

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
