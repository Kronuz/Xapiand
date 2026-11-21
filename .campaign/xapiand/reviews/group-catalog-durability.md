# group-catalog review: durability

Commit: strata:096dbd042c79bba2ad1aa319b9e421552a31f16d
Verdict: CHANGES
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261121-103914-1a21ad: the suite at 096dbd04, exit 0.
- 261121-103852-b1cb8c: group_catalog_test at 096dbd04, exit 0 (A1, A2, A3 on the shutdown path).
- 261121-103856-a3faa5 (strata:053aede3, close skips the checkpoint): fails with "the bundle covers
  everything applied: P_b 0 of 10". A2's test catches it.
- 261121-103905-bc298a (strata:1d3155e7, the close in the commit hook): fails with "past the
  deadline at: the catalog's shutdown". A3's deadline catches the self-wait. These jobs are enough
  for the shutdown close. They say nothing about the sweep, bound, replace or failed-open paths.

## What I checked myself

- I read every path that closes or destroys the slot's index in catalog.hh: run_close (sweep,
  bound, shutdown), run_replace, run_stuck_retry, retire, the catch in run_open and ~catalog.
- run_close and run_replace call close_payload after `chore_scope` ends, right before
  `index.close()` (catalog.hh:2128, :2212). I confirmed that every run_close and run_replace runs
  on an executor lane, either from `post` or from a timer that posts (catalog.hh:1125, :1845).
- The commit hook is a bare `index.commit()` everywhere it is called (:1983, :2072, :2092,
  :2204). It never waits on the group.
- One directory, one open: `resolve` makes the IO once (:1656), and `open_index` and
  `opening::io` lend that same IO. `borrowed::emplace` resets before it reopens. No test opens a
  directory twice while it is still open.
- The test migration (096dbd04) changes construction sites and comments only. No assertion is
  removed or weakened. The two deleted A6 mutants targeted code the group no longer has.
  `destroy-keeps-member` was already stale at main (`~group` at main is three lines), so this
  quest did not break it.
- D16 with a bare commit: the shutdown commits (P_H moves up) before close_payload checkpoints.
  The new state is P_H above the synced file bytes, because a bare commit does not sync the file
  set. The docs say replay rewrites them. No existing test crashes in that state.
- Probe job 261121-132654-daff1f (TSan, Debug) is still queued; it has no result yet. It runs
  `.scratch/xapiand/review-group-catalog-durability/run_probe.sh`, which generates
  `review_probe.cc` from this commit's group_catalog_test harness with four scenarios:
  1. `attach`: groups constructed on the catalog's lane while another group commits on the loop.
  2. `failed-open`: the index directory is lost under a bundle, so recover fences and the catalog
     destroys H on the lane.
  3. `sweep`: an eviction close through close_payload, then a reopen.
  4. `bare-commit-crash`: a bare commit, power off, a crash with 16 seeds, then recovery checking
     every value.

  Rerun it as `campaign job submit --quest group-catalog --level light --cpus 4 -- <that script>`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | yes | defect | **Destroying the group after a failed open is unsafe.** When open() fails (recover fenced, P_b > P_H, or a Driver error), it fences, closes the row and rethrows (group.hh:238-249). It never leaves the loop's table and never waits for the chore to go idle. The catalog then destroys H on its lane (catalog.hh:1664-1668). `~group` sees `attached_` still true and calls `loop_->detach` off the loop. That races the loop's `members_` (a plain std::map, group.hh:105-111, "calls are the loop's"). The chore's `drain()` tail after the failed recovery item can also still be touching `mu_` and `queue_`. Ruling 4 fixed this race for close() but not for a failed open. The same applies when open_blocking's guard throws. | group.hh:238-249, :184-190; catalog.hh:1664-1668; probe 261121-132654-daff1f `failed-open` (pending) | open()'s failure path tears down as close does: detach on the loop, refuse items, `co_await chore_idle()`. Alternatively, open_blocking runs close() on failure. Test: a catalog pin whose recover fences (fresh directory under a bundle), run under TSan, then a second pin. |
| R2 | yes | gap | **The constructor joins the loop's table on the catalog's lane.** `group(...)` calls `loop_->attach` in its body (group.hh:178), and the catalog's open_payload constructs H on a lane. With many groups on one loop, which is journal-replaces-wal's shape, that insert races the loop's `find()` for other groups' notices. This contradicts the same ruling 4 the quest relies on for close. Until probe `attach` reports, I cannot rule out a data race. | group.hh:105-111, :172-178; group_catalog_test.cc open_payload; probe 261121-132654-daff1f `attach` (pending) | attach in open() on the loop, and keep `attached_` false until then. Test: two or more groups through the catalog, one writing while another opens, under TSan. |
| R3 | yes | gap | **Only the shutdown close is tested.** Sweep eviction, the wake-bound close and the replace close each call close_payload (catalog.hh:2128, :2212), but no test reaches them (`sweep_period_ns` is one hour in the test). Eviction is the common close for many indexes per host. A mutant that drops close_payload from run_replace or from the sweep branch would pass the suite. | group_catalog_test.cc: `sweep_period_ns = 3'600'000'000'000ull`; probe `sweep` (pending) | Add a sweep-close case and a replace-close case (a fenced index) to group_catalog_test, each checking the bundle at everything applied and a reopen that recovers it. |
| R4 | yes | gap | **No test crashes after a bare commit.** With D169 the catalog's bare commit can make P_H durable over file-set bytes that are not yet synced, a state that did not exist before. The docs claim D16 replay rewrites those bytes. The mechanism is plausible, but no test crashes with P_H > P_b reached through a bare commit. | docs/mechanisms/group-catalog.md "The hooks"; probe `bare-commit-crash` (pending) | Add a crash test: writes, bare commit under `chore_scope`, power cut with several seeds, then recovery checks every acknowledged value and its bytes. The probe scenario can be the starting point. |
| N1 | no | defect | **A refused close after close_payload leaves a dead group in an open slot.** If a sweep close's `index.close()` throws with `!closing()` after close_payload has run, `close_refused` returns the slot to its prior state with H already torn down. Pins until the next pass get a group that answers `closed`. No data is lost. | catalog.hh:2128-2141 | Treat a refusal after close_payload as stuck or retrying, not as `close_refused`. |
| N2 | no | defect | **A blocking-guard refusal leads to a destroyed live group.** When close_blocking refuses (on the loop or the chore, nothing started), close_payload's policy still closes the index and retire destroys a live group. A host misconfiguration that would have hung becomes a use after free. | group.hh:536-539; catalog.hh:2229-2242 | Make the refusal fatal (terminate or assert), or make it a failure the catalog does not close through. |
| N3 | no | gap | **The guard misses schedulers without `runs_here`.** For an inline_scheduler, `runs_here()` is false (task.hh:228), so close_blocking on a pumping thread hangs instead of refusing. | group.hh:537 | Also check `task::current() == loop_->scheduler()`. |
| N4 | no | gap | **The group's chore writes the index with no pin held.** Follower applies, ack::none applies and Driver-triggered checkpoints call the borrowed index with no pin, which the catalog's CT-R15 check reports in Debug builds. | index.hh:3143-3145; catalog.hh:1055-1060 | Exempt the group's lease holder, or document the exemption in group-catalog.md. |
| N5 | no | gap | **The sweep never evicts a group with uncommitted applies.** `close_ready` requires nothing uncommitted, and only the group's own checkpoints commit, so an idle group with applies since its last checkpoint is never evicted. This belongs to the design or system lens. | catalog.hh:2081-2099 | The owner decides whether the sweep should commit for groups. |
| N6 | no | nit | **Two leftovers.** replication-group.md's status line still says the catalog's ownership follows. mutants.py's `destroy-keeps-member` is stale (it was already stale at main). | journal "Open for the lead" | Update both. |

## Not run

- The probe job 261121-132654-daff1f has no result yet: R1, R2 and R4 stand on code reading
  alone. R1, R2 and R4 are blocking because they are consequential and unsettled, not because a
  run showed them. A clean TSan run of `attach` and `failed-open`, plus a passing
  `bare-commit-crash`, would turn R2 and R4 into tests to add rather than defects.
- No ASan build, no Linux run, and no voters (multi-node) case through the catalog.
