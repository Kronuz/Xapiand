# faultlab-core: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 3: recorder.h (F10) + tests.

## Next

1. recorder.h (F10) + tests (A5 verify).
2. scenario.h, events.h (F11, F12).
3. Python half: history.py (monotonic, seal, set-aside), proxy.py, scenario.py; selftest, proxy_test (A5).
4. README, docs/ (A6).
5. Remaining mutant jobs: additions (A1, policy), fold-mutates, kill-keeps-leases (A4), forced-skips-draw (A1).

Worktree: /Users/kronuz/code/Kronuz/faultlab-wt/xapiand/faultlab-core (faultlab repo, branch quest/xapiand/faultlab-core).
Done: step 1 faultlab:bfce6a8; step 2 faultlab:d9d420b, faultlab:(policy stream test) — tests faultfs, policy, seeds, sector green locally.
Mutants are local refs mutant/faultlab-core/<name> (one commit on step 2's head each), made by
.scratch/mutant.sh; all seven fail their test locally (.scratch/check_mutants.sh).
Jobs: verify build-test 261118-003124-8f88ab (A1-A4); fails-without: link-orphan 261118-003126-596e36 (A4),
no-sector-cut 261118-003131-9c42c3 (A3), stream-shift 261118-003136-4b6402 (A2).

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
