# x-pool: The index registry becomes the catalog's payload

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x-pool`.

## Outcome

Xapiand adopts strata's cold-tier catalog: the index registry is the catalog's payload (`H`), every
request pins its index before it takes a view, a cold key's pin opens it and replays the WAL, an
owed index's pin wakes it, idle indexes close only by the catalog's sweep, uncommitted writes are
committed as a catalog chore at the sweep's close and at shutdown, FLUSH works by snapshot, and a
write to a demoted `defer_io` index retries after one `promote()` (`docs/design/x-pool.md`
revision 2, steps 2 to 4; step 1 landed as `x-pool-step1`). Spec: strata `docs/design/cold-tier.md`
"Xapiand adopts it" on `design/cold-tier`, with the architect's host-interface rulings (strata
`docs/reviews/cold-tier-xpool-host.md`).

## Acceptance criteria

- A1 XP1 pin before view: a pin awaited under a held view aborts (debug); every request path pins before its views (x-pool design XP1): `test_reactor_requests` ("a pin"), the docs E2E on an asserts build, mutant "no check in `pin`" killed
- A2 XP2 a pin opens or wakes: a cold key's request pin opens, replays the WAL and hands the pin with the replayed documents readable; an `owed` index's pin wakes it with `unleased_maps()` zero; a pin after `reopen` of a fenced index reads the replayed documents (x-pool design XP2): `test_pool`, mutants "deliver before the replay" and "no `reopen` on `index_fenced`" killed
- A3 XP3 the idle path is gone: no idle-close code path; an unpinned idle index closes by the sweep only, a pinned one never (x-pool design XP3): `test_pool` with the sweep at 1 kHz and a held pin; mutant "unpin early" killed
- A4 XP4 commit as a catalog chore: a write left uncommitted is committed (WAL COMMIT line, revision + 1, blobs synced) by the sweep's close and by `shutdown()` (x-pool design XP4): `test_pool`; mutant "the default chore" killed
- A5 XP5 `map_owed`: a write to a demoted `defer_io` index applies after one `promote()`; a third owed answer is `errc::internal` (x-pool design XP5; step 4 with the `unleased_maps()` assertion, the budget and CT-R39's clamp): `test_pool` with a strata stub that answers owed forever; mutant "no retry" killed
- A6 XP6 sanitizers: zero TSan, ASan and UBSan reports in ctest, the docs E2E, `graceful_shutdown_check.sh` and `path_boundary_check.sh` (x-pool design XP6; Linux on the local VM per D24): the `asan` and `tsan` checks, and a Linux VM job
- A7 XP7 no blocking: `async_check --strict --owed --extern strata=<pin>` reports 0 owed (x-pool design XP7): the `build-test` check
- A8 Step 2 review condition 1 (CR1): a root destroyed while it awaits a pin leaves no stranded pin; the review's `destroyed_awaiter` probe is a regression test (`join` true, ASan clean), run under ASan and TSan (`docs/reviews/phase7c-x-pool-step2-code.md`): the regression test in the `asan` and `tsan` checks
- A9 [artifact] Step 2 review condition 2 (CR2): the design rules whether `pin()` takes the request's token and whether a task's second pin of a key reuses its first: `docs/design/x-pool.md`
- A10 CR3: a destroyed root that held a pin lets `finish()` then `join()` return true (pool-level property test) (x-pool ledger, Next 1): `test_pool`
- A11 CR4: a cancelled resident pin throws `task::cancelled` and FLUSH ends; `pin_chore` no longer swallows it (x-pool ledger, Next 1): `test_pool`
- A12 `DatabasePool::pin` and `pin_chore` pass the request's cancel token (x-pool ledger, Next 1): `test_pool` and `test_reactor_requests`
- A13 [artifact] CR5's behavior changes stated in the design (sweep defaults, every open replays the WAL, FLUSH commits and no longer closes, negative entries after a 404 scan), with numbers in step 4's budget (x-pool ledger; design D13): `docs/design/x-pool.md`
- A14 CR6: the hostless path (`engine::open_local`, `Core::owned`) holds its index through the catalog, or the design records why not (x-pool ledger, "CR6 owed"): `test_pool`, and the design text
- A15 Wide search pins through `IndexPin`: `search_batch` pins each batch's indexes through the pool, K clamped to `soft_open` (x-pool ledger, done criterion "After wide-search lands"): `test_pool` and `harness/wide_search_check.py check`
- A16 Xapiand pins strata at a main commit holding cold-tier items 5 and 5b (x-pool ledger, Next 3): the `build-test` check at the landed `CMakeLists.txt`
- A17 Docs E2E on strata: every assertion passes (644 or more), 0 regressions: the `e2e` check
- A18 [artifact] How it works: the registry on the catalog and the pins' lifetimes: `docs/mechanisms/index-registry-on-catalog.md`

## Contracts

Consumes strata's catalog host interface (`strata/core/catalog.hh`: `pin`, `try_pin`, `pin_chore`,
intents create and resident, `reopen`, `shutdown`, `snapshot`, cancel tokens) as of strata main at
strata:5d742b24. Provides `DatabasePool::pin`/`pin_chore` returning `IndexPin` with the request's
token, which `x7-gate`'s T13 row and `name-index` build on.

## Decisions

- Adopts `phase7c/x-pool-step3` (Xapiand:0638e61fd), which contains `phase7c/x-pool`
  (Xapiand:b6b9718c2, step 2 and its review) and step 3; its last four commits (repin to strata
  main, tokens to pins, cancellation tests, shutdown report) came after the ledger's last update
  (the finisher stopped at the quota limit): the contributor verifies them first.
- Prior evidence at Xapiand:0638e61fd, outside git (the finisher's last checks, 2026-11-04 15:55 to
  17:30 PST, `~/code/Kronuz/Xapiand-wt/x-pool-step3/.scratch/` on the laptop; no ledger records them):
  the docs E2E is NOT GREEN in the plain build (54 failing assertions, `plain-e2e.log`) and under TSan
  (48, `tsan-e2e.log`), GREEN under ASan (`asan-e2e.log`); the plain run's first failures are "Load
  sample data" (the RESTORE gives no response; `doc_count` 679 of 1,000), then ordering checks. Alone,
  a RESTORE of the same 1,000 accounts answers 200 with every document indexed (`restore-probe/`).
  Mutants t4 to t7 of `mutants4/mutants.py` are killed (`mutants4/run.log`); TSan `blocking_guard` (14
  checks, 0 reports), graceful shutdown and the wide-search check pass (`chain4.log`). A17 starts from
  the E2E failure.
- The step 2 code review (`docs/reviews/phase7c-x-pool-step2-code.md`, APPROVE WITH CONDITIONS at
  Xapiand:849dee73c) is history: its conditions are A8 and A9 and its findings CR3 to CR6 are
  A10, A11, A13, A14. One correctness review covers steps 2 to 4 at the landing commit.
- Open (lead): the C24 reorder cannot be shown at the manager (ledger, findings); A10 is the test
  that keeps it.
- Open (lead): whether `name-index` (the ordered name index, "designed with X-POOL step 2 or
  later") rides on this quest; it is planned as its own quest.

## May land early

None: steps 2 to 4 change index lifetime and durability on every request path.
