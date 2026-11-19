# wide-search-loop: journal

State: ready for review (re-check): every criterion has evidence; A2 Linux's 1,024 us bound is literally unmet, see Blocked on
Quest: .campaign/xapiand/quests/wide-search-loop.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

CI red at 5b206efde (tests and allowlist only; the mechanism is unchanged), fixed at Xapiand:6a99cf152:
- test_wide_search.cc:559 `CHECK(cancelled)`: cause is the start order, not timing. Without symmetric transfer (GCC,
  and the sanitizer builds) awaiting a task queues it on a per-thread trampoline, so a `when_all`'s children do not
  reach their first wait in order: `cancel_then_release` ran before the batches had queued, the head finished, and the
  cancelled batch found the line free (a free lock is taken whatever the token) and took it. macOS clang transfers
  symmetrically, so it passed locally. The waiting-batch test passed vacuously the same way. Both now spawn on one
  `task::inline_scheduler` and run it dry between steps (no `when_all`, no sleeps). Evidence: the old shape on gcc-13 in
  the Linux VM fails (cancelled=0, got=1: 261119-041053-47f93c), the new shape passes -O2 and ASan (261119-041250-f42e9e);
  the real binary x50 on macOS: 261119-041251-1cffc2 (22,317 checks each run). CI on the push is the sanitizer check.
- async_check --strict: one allowlist row for `wide_search.cc` mutex_ (count 3: take's check, held, give: O(1), no I/O;
  they do run on the loops, and the room event is set outside the lock). `--strict --owed` rc 0, test_gate_tools.py OK.
- FreeBSD gate7c_rows: the failing rows were t1_strict_owed and gate_tools, both the same three async_check hits (fixed
  by the row). Master's a465288dc fails a different row, t3_crash_asan ("strata D recovered 4501, 1 wrong"): pre-existing,
  the lead's.

Session 5, last step: all ten jobs read (design doc "Branch measured again"). Head recorded below.
- Suite at the merged head 261119-015540-f34736: exit 0, 52/52.
- A2 Linux exclusive (015840-52e913 / 015843-118f54 at 2,500; 015846-7c0eec / 015849-58ebd1 at 10,000): http0 / http1
  max since start 60.5 / 76.7 ms -> 6.95 / 2.77 ms (2,500), 329.2 / 299.3 -> 7.38 / 7.39 ms (10,000); window max
  <= 65.5 / 81.9 ms -> <= 2.05 / 2.05 ms and <= 393 / 2.56 -> <= 3.07 / 3.58 ms; p99 1.5-2.0 ms every arm. Control loop alone: window
  max <= 2.05-2.56 ms, max since start 21.5-39.7 ms, p99 1.8-2.0 ms. The guest's idle wake (about twice 1,024 us)
  explains 2,500 (branch window = control's) but not all of 10,000 (3.07 / 3.58 vs 2.05 ms).
- A2 macOS exclusive: 2,500 max since start 105.4 / 75.9 -> 2.08 / 2.05 ms, 10,000 281.0 / 232.0 -> 2.05 / 2.08 ms;
  control window max <= 2.56 ms; p99 448-768 us (branch) vs 384-1,280 us (master).
- F3 (568ffe, exclusive, 3 reps): light p99 master 1.51 / 0.68 / 1.09 ms vs branch 1.86 / 1.58 / 1.57 ms, T5 bound
  3.09-3.21 ms: PASS on all six; max 473 / 1,299 / 1,982 ms vs 9.9 / 5.3 / 9.4 ms.

Session 5 (resumed). Read the finished session-4 jobs (below); merged master (Xapiand:0b7e4b69a, src change in
`src/database/schema.cc` only); docs at Xapiand:9b474d811; suite at that head 261119-015540-f34736 (light, 120 min).
Jobs read:
- A2 Linux (VM, strict, beside 6-8 light holds), http0 / http1 max since start: 2,500 master 179.9 / 162.6 ms -> branch
  19.7 / 14.3 ms (9-11x lower); 10,000 master 558.1 / 475.8 ms -> branch 10.4 / 8.8 ms (54x lower). p99 2,048 us
  (master) vs 2,048-3,584 us (branch); control loop max 17.7-29.9 ms in every run (the guest's floor). Search p50
  2,611 -> 2,322 ms (2,500), 7,846 -> 6,897 ms (10,000). The branch's max is 8.6-19x over A2's 1,024 us: not met
  literally on these runs, and below the same run's control loop; see Blocked on.
- A3 strict (macOS): names, light p99 / max: master 13.64 / 1,336 ms (T5 bound 2.32 ms: FAIL, exit 1), branch 1.91 /
  2.55 ms (bound 2.27 ms: PASS); pattern: master 2.81 / 1,863 ms, branch 2.14 / 19.6 ms. Alone p99 0.66 / 0.64 ms.

Session 4 (correctness review CHANGES at Xapiand:3e2edbef4, `.campaign/xapiand/reviews/wide-search-loop-correctness.md`).
Master merged (no src change). Fixes:
- F1 (blocking): Xapiand:9d827de68. `wide_search::resolve` takes `task::budget*`; the call site passes it only when
  `query_field.patterns` (search and count), so a list HEAD, list :commit or refused list write resolves whole, as on
  master. Tests first, at Xapiand:6fe65e78a: test_node `test_a_half_closed_commit_of_a_list_commits_durably` (child:
  write applied to hcl_kept without WAL or autocommit; half-closed `POST /hcl_new,hcl_kept/:commit` whose hcl_new
  settings load waits on the reserved index's held turn while the EOF lands; 200, then _Exit; restart finds the write),
  and HEAD /hc,hc2/1 and POST /hc,hc2/:commit rows in `harness/wide_search_halfclose.py`.
- F2: test_node `test_a_cancelled_wide_search_gives_its_count_back` (Xapiand:d9cc31863): `--concurrent-search-indexes 1`,
  ordered by `pins().held()`, so a removed take or a leaked Hold fails it. Pins unit tests (Xapiand:78198eea6) run
  their batches as one thread's `task::when_all` (children start in order up to their first wait): no sleeps, and the
  cancelled-batch case can no longer hang.
- F5: Xapiand:78198eea6. `search_batch` counts distinct shards before the take; `Pins::try_take` removed. Test:
  test_node `test_a_wide_batch_counts_a_repeated_index_once` (Xapiand:d9cc31863, fails before the fix).
- Evidence (step 4): 204934-64d3ad ran in slot `xapiand-wide-search-loop` (default), so every macOS A2/A3 job is now
  submitted with `--slot wide-search-loop`. 204950-07c772: `Exec format error` on the VM's build-master binary, linked
  at 05:04 just before the build job's stop (`vm.sh stop` = poweroff, sleep 5, kill qemu): unsynced guest writes lost.
  Xapiand:e649ebcb1 syncs the guest before every stop, relinks each arm, fails the build unless the binary is ELF, and
  builds the master arm from the branch's whole src diff reversed (8e5040da8 alone no longer reverts cleanly).
  Submission scripts: `~/.scratch/xapiand/wide-search-loop/submit-{macos,vm}.sh`.

| Job | Level | For | What |
|---|---|---|---|
| 261118-214334-70753d | light | A4 | suite at Xapiand:e649ebcb1: PASS, 44/44 (test_node's three new cases, test_wide_search's when_all cases) |
| 261118-214345-e0caec | light | A4 | test_node x5 at the head, 0 only at 5/5 |
| 261118-214405-d4ad8f | light | A4 fails-without | F2: test_node cancelled-wide case with the take deleted |
| 261118-214416-55f6c4 | light | A4 fails-without | F1: test_node list-commit case at Xapiand:6fe65e78a |
| 261118-214430-1d4cc0 | light | A4 fails-without | F5: test_node repeated-index case at Xapiand:d9cc31863 |
| 261118-214441-5d5798 | light | | builds build-hc-{master,mut,branch}; mutant passes the budget unconditionally |
| 261118-214452-f8d1d8 | light | A4 | F1: half-close probe, branch vs master |
| 261118-214453-f4a3ac | light | A4 fails-without | F1: half-close probe, mutant vs master |
| 261118-214455-d04574 | light | A4 | F1: reviewer's halfclose_multi.py, branch vs master |
| 261118-214548-17cbe8 / 214550-dea844 | light | | slot wide-search-loop: build-probe (branch src) / build-prof (master src) |
| 261118-214551-71a78d / 214554-8a79f5 | exclusive | A2 macOS | SR3 2,500 before / after |
| 261118-214552-695322 / 214555-113315 | exclusive | A2 macOS | SR3 10,000 before / after |
| 261118-214557-802978 / 214558-d15a54 | strict | A3 | starve 64 before / after |
| 261118-214617-f0c429 | light | | VM build both arms |
| 261118-214618-1e9519 / 214620-79eb0b | light | | VM create 2,500 / 10,000 |
| 261118-214621-f82004 / 214622-52318a | strict | A2 Linux | SR3 2,500 before / after |
| 261118-214624-c6c5de / 214625-59b5ea | strict | A2 Linux | SR3 10,000 before / after |
| 261118-210116-568ffe | exclusive | F3 | reviewer's A3 ABABAB (read when it runs) |

Cancelled, mis-submitted (zsh did not split the options): 214509-a7a619 .. 214520-cb6d6e; 214418-15eb96 (rechained).

Earlier sessions:

Session 3: master merged (Xapiand:700ffc0c3, no src change from master), branch pushed for CI (A4). Session-2 jobs read
into `docs/design/wide-search-loop.md` "Branch measured": at 10,000 exclusive the http loops' max 322 / 373 ms (master)
-> 2.07 / 2.07 ms (branch); S2 64 names light max 1,140 -> 3.4 ms, pattern p99 17.6 -> 1.9 ms; the 100,000-entry
resolver cache changes nothing (hypothesis dropped, `src/index_resolver_lru.*` untouched). Open: branch p99 at
10,000 exclusive 1.5-2.0 ms vs master 384-448 us (idle wake slack or short turns: the beat-on A/B decides).

Lead R6 (Xapiand:86a50e08e): A2's 1,024 us bound is read on Linux (strict jobs in the VM at ~/.scratch/vm-document-gate,
port 18782); on macOS each http loop's max is no worse than the same run's control loop; the beat A/B decides the p99
rise, and longer turns fail A2 on either OS. `harness/wide_search_vm.sh` (Xapiand:57818c369) boots the VM under a mkdir
lock, builds both arms (master = 8e5040da8's src reverted), creates the indexes, runs SR3, and stops it on exit.

Session-3 first submissions failed or were redone: the VM build 114932-476f73 hit the 6 GiB guest's OOM killer at
-j4 (chain skipped; Xapiand:d3411d2aa builds at -j2); A3 113018/113056 and the macOS 2,500 and beat jobs ran in a fresh
checkout without the slot's binaries and data (now `--slot wide-search-loop`); suite 112500-d8889b failed test_node
(`test_a_put_whose_client_half_closes_completes_durably`, test_node.cc:1169/1175), unrelated to search; f7bb71 runs
it at master to show whether it is pre-existing.

Session 3, last step (lead: cffd0190a proven). Read before the fix (reads unchanged by it):
- Beat A/B at 10,000, exclusive: master http p99 320 us, max 267 / 333 ms; branch p99 448 us, max 915 / 1,011 us
  (6328f9, 2ba112): the p99 rise without the beat is idle-wake delay, not longer turns (R6's question).
- macOS 2,500, exclusive: http max 73.5 / 77.8 ms -> 2.06 / 2.33 ms, control 2,560 us bucket (09c53e, c9dd00).
- A3 strict: master 64 names light p99 1.26 / max 928 ms PASS on p99 (bb97fa); branch p99 2.85 vs bound 2.39 ms
  FAIL, max 16.5 ms (243767). Session 2's branch run passed (1.69 / 2.63): T5's p99 is noisy at strict; the rerun
  decides, exclusive if it misses again.
- VM create 132819-66b8f7 failed: no data directory (fixed, Xapiand:adc71378b).

Reruns at the head (Xapiand:adc71378b):

| Job | Level | For | What |
|---|---|---|---|
| 261118-204934-64d3ad | light | | build-probe (branch src) |
| 261118-204936-89b870 | light | | build-prof (master src: the branch's src diff reversed) |
| 261118-204937-0379fa / 204943-b5ecc2 | exclusive | A2 macOS | SR3 2,500 before / after |
| 261118-204938-0b8425 / 204944-67b125 | exclusive | A2 macOS | SR3 10,000 before / after |
| 261118-204940-5102a8 / 204945-b054bc | strict | A3 | starve 64 before / after |
| 261118-204941-e7215b / 204947-345e6f | exclusive | R6 p99 | beat A/B at 10,000 before / after |
| 261118-204948-5f181c | light | | VM build both arms |
| 261118-204950-07c772 / 204952-37627c | light | | VM create 2,500 / 10,000 |
| 261118-204953-ab2050 / 204956-040046 | strict | A2 Linux | SR3 2,500 before / after |
| 261118-204958-a9c88b / 204959-bf947d | strict | A2 Linux | SR3 10,000 before / after |

Half-close, answered (probe 170529-87c0b4, rc=3): master answers a half-closed GET /hc/1 200 x5 and refuses :search
and :count 503 x5 (pre-existing: http's EOF policy plus master's own checkpoints; the lead takes it to the owner, not
changed here); the branch also refused GET /hc/1 (introduced, as the PUT). Fix Xapiand:cffd0190a: only a list of names
checkpoints (`paths.size() > 1`); a single name resolves whole, reads and writes, as on master; replaces 9a3cfeccf's
writes-only rule. Bisect 170254-9f8b5a: test_node 0/5 at 8e5040da8 alone (the call sites). Suite 170300-52f4dd at
9a3cfeccf passed. Probe takes `--baseline` and fails only on a read the baseline answers.

| Job | For | What |
|---|---|---|
| 261118-174748-eec809 | | test_node x5 at cffd0190a, exits 0 only at 5/5 |
| 261118-174750-e0b022 | A4 | fix cffd0190a: test_node x5 174748-eec809, probe 174834-15710d, fails-without 174835-9a2a47 / 174903-ca5c07, suite 174750-e0b022 pending; CI running |
| 261118-174832-f05da1 | | builds: master src, branch src, mutant (`if (wide)` -> `if (true)`) |
| 261118-174834-15710d | A4 | fix cffd0190a: test_node x5 174748-eec809, probe 174834-15710d, fails-without 174835-9a2a47 / 174903-ca5c07, suite 174750-e0b022 pending; CI running |
| 261118-174835-9a2a47 | A4 fails-without | half-close probe on the mutant |
| 261118-174903-ca5c07 | A4 fails-without | test_node on the mutant |

Gate check for the lead (state is the lead's): `harness/wide_search_halfclose.py BIN OUT PORT --baseline MASTER_BIN`,
the command of 174834-15710d after the build of 174832-f05da1.

Lead's question (half-closed reads). Where the token is set: Kronuz/http at the pin eb08b59e6aa2, `http_asio.h`
`ConnectionInput::completed` fires the request's watch on any read error but our own abort, EOF included, and the
watch calls `Request::cancel(http::disconnected)` (`http_message.h` CancelWatch). It does not tell a half-close from a
reset: by design (`http_asio.h:426-431`), only deferred/owned-stream replies ignore EOF. `SearchService::handle`
(`search_views.cc:3174`) asks for `hreq.cancel_token()` on every request, which engages the lazy watch at once, and
builds the request's budget on it. Probe `harness/wide_search_halfclose.py` (Xapiand:0ee651e46): GET doc, search and
count, plain vs half-closed x5. Job 261118-170529-87c0b4 runs it on master src and branch src (same tree otherwise):
if master misses too, it is pre-existing (http's policy plus master's search checkpoints); if only the branch misses,
the branch's read checkpoints (resolve, search_batch) introduced it.

test_node regression, found and fixed (Xapiand:9a3cfeccf). Controls: test_node alone x5 passed 5/5 at master
(150319-fc7064), 0/5 at c59b6e739 (150321-7fa506), 5/5 at b46f8e262 (150323-09adf2). Cause: 8e5040da8 routes every
view's name resolution through `wide_search::resolve`, whose `budget.checkpoint()` throws `cancelled` when the token
is requested (task.hh, budget::checkpoint: await_ready is true on a requested token, await_resume throws). The
half-closed PUT's EOF requests the token before `write_document_view` resolves, so the write ends 503 before it is
admitted. Fix: only reads checkpoint (`if (!writable)`); a write view resolves one index. Regression test: test_node.

| Job | Commit | What |
|---|---|---|
| 261118-170254-9f8b5a | 8e5040da8 | bisect: test_node x5 (expect failures: the call sites alone) |
| 261118-170257-d8055d | 9a3cfeccf | fix: test_node x5, exits 0 only at 5/5 |
| 261118-170319-9e4695 | d986bcdd7 | `--fails-without --for A4`: test_node fails before the fix |
| 261118-170300-52f4dd | 9a3cfeccf | suite (A4) |

The fix changes src (writes only), so the A2/A3 jobs queued at d3411d2aa/c59b6e739 run pre-fix binaries; reads are
unchanged, but the tool will ask for reruns at the landed commit (macOS build-probe rebuild and the VM build).

test_node failed 2 of 2 suite runs on the branch (d8889b at 700ffc0c3, f1f8e1 at d3411d2aa): the half-closed PUT is
answered 503 and the restart finds no write (test_node.cc:1169, :1175); master's CI passes it. The master control
f7bb71 never reached the test (wrong target). Controls, test_node alone x5 each, chained in the slot (`xapiand_test_node`):

| Job | Commit | What |
|---|---|---|
| 261118-150319-fc7064 | master | test_node x5 |
| 261118-150321-7fa506 | branch head c59b6e739 | test_node x5 |
| 261118-150323-09adf2 | b46f8e262 (before the call sites and the merge; its suite passed 44/44) | test_node x5 |

If master is 5/5 and the head fails: b46f8e262 tells whether the call sites (8e5040da8) or the merge brought it;
bisect from there and fix. The branch's src touches only search (`search_batch`, `resolve_index_endpoints`).

| Job | Level | For | What |
|---|---|---|---|
| 261118-132935-f1f8e1 | light | A4 | fix cffd0190a: test_node x5 174748-eec809, probe 174834-15710d, fails-without 174835-9a2a47 / 174903-ca5c07, suite 174750-e0b022 pending; CI running |
| 261118-132936-f7bb71 | light | | test_node at master (pre-existing check) |
| 261118-132928-09c53e | exclusive | A2 macOS before | SR3 2,500, build-prof (master src) |
| 261118-132932-c9dd00 | exclusive | A2 macOS after | SR3 2,500, build-probe (branch src) |
| 261118-051433-c76a1c | exclusive | A2 macOS before | SR3 10,000 master src (reused: only merges since) |
| 261118-051434-89f2f5 | exclusive | A2 macOS after | SR3 10,000 branch src (reused likewise) |
| 261118-132929-bb97fa | strict | A3 before | starve 64, master src, exits on T5's verdict |
| 261118-132933-243767 | strict | A3 after | starve 64, branch src, exits on T5's verdict |
| 261118-113058-7cb4e4 | strict | A1 | attribution rerun at 700ffc0c3 (done) |
| 261118-132931-6328f9 | exclusive | R6 p99 | SR3 10,000 master src, XAPIAND_LAG_SPIN=1000000 (250 us beat keeps loops awake) |
| 261118-132934-2ba112 | exclusive | R6 p99 | same, branch src |
| 261118-132813-32aa11 | light | | VM: apt deps, build both arms at -j2 |
| 261118-132819-66b8f7 | light | | VM: create 2,500 gate indexes |
| 261118-132826-b0ada4 | light | | VM: create 10,000 gate indexes |
| 261118-132830-475ea9 | strict | A2 Linux before | SR3 2,500, master src |
| 261118-132836-3c231e | strict | A2 Linux after | SR3 2,500, branch src |
| 261118-132842-a03818 | strict | A2 Linux before | SR3 10,000, master src |
| 261118-132847-f885d2 | strict | A2 Linux after | SR3 10,000, branch src |

macOS builds are the session-2 binaries (f5a684 build-probe, 692132 build-prof): the tool matched them at 700ffc0c3.

Session 2 (read; numbers in the design doc; lead widened the set to the call sites and `src/index_resolver_lru.*`): call sites applied as code
(Xapiand:ca68b2e39, patch deleted); harnesses take `--node-arg`; the stall sampler beats on a 250 us timer and prints
the loop thread's CPU time over each late beat (Xapiand:1ccd11a5e): on CPU = a long turn, off CPU = preempted/blocked.
Open misses: T5 at 64 clients (light p99 5.3 ms vs 4.2 ms), T4 at 10,000 (max lag 6.6-9.2 ms vs 1 ms).

| Job | Level | What |
|---|---|---|
| 261118-050242-f5a684 | light | build-probe at 1ccd11a5e (branch src + new sampler) |
| 261118-050258-4c6f40 | strict | T5 control: starve 64, default cache |
| 261118-050259-a1757e | strict | T5 hypothesis: starve 64, `--resolver-cache-size 100000` |
| 261118-050300-6ee958 | strict | T4 control: gate 10,000, default cache |
| 261118-050301-091601 | strict | T4 hypothesis: gate 10,000, `--resolver-cache-size 100000` |
| 261118-050302-0cee31 | strict | T5 attribution: starve 64, `XAPIAND_LAG_SPIN=2000` |
| 261118-050303-e61fcf | strict | T4 attribution: gate 10,000, `XAPIAND_LAG_SPIN=2000` |
| 261118-051427-692132 | light | build-prof: master's src (8e5040da8 reverted in the job), same sampler |
| 261118-051429-5d9b36 | strict | A3 before (lead msg): starve 64 at master src, same sampler inert |
| 261118-051431-0d7e48 | strict | A2 before (lead msg): gate 10,000 at master src, same sampler inert |
| 261118-051433-c76a1c | exclusive | T4 host-or-engine: gate 10,000, master src |
| 261118-051434-89f2f5 | exclusive | T4 host-or-engine: gate 10,000, branch src |

Lead (session 2): A2/A3 before and after use binaries with the same timer-beat sampler, so session 1's before
jobs are superseded by 051429/051431; T4 is decided under exclusive before it is called engine or host.

Session 1:

Attribution on current master (src unchanged at Xapiand:92bcc001c; harness only). Slot `wide-search-loop`:
build-prof (RelWithDebInfo, -fno-omit-frame-pointer), data in build-wide2500, build-wide10k, build-starve2000.

| Job | Level | For | What |
|---|---|---|---|
| 261118-022548-0f3ded | light | | build-prof |
| 261118-022554-5856e1 | light | | create 2,500 gate + 2,000 starve indexes |
| 261118-022559-f653ff | light | | create 10,000 gate indexes: timed out at 30 min (5,779+ made) |
| 261118-033556-81775d | light | | completes the 10,000 (`--create-only --reuse`) |
| 261118-022612-9e4e4d | strict | | 2,500 gate + `sample` 10 s (attribution) |
| 261118-022616-80ea60 | strict | A2 before | 2,500 gate, SR3 |
| 261118-022618-928443 | strict | A3 before | starve probe 16,64 clients |
| 261118-033603-400442 | strict | A2 before | 10,000 gate + `sample` 15 s (replaces cancelled 4ad293) |
| 261118-024337-7a9aec | light | probe | build-probe: Xapiand:c33b7cc79 + `harness/wide_search_callsites.patch` |
| 261118-025903-1d4a33 | strict | probe | 2,500 gate + sample on build-probe |
| 261118-025905-94c734 | strict | probe | starve 16,64 on build-probe |
| 261118-034210-a84949 | light | A4 | fix cffd0190a: test_node x5 174748-eec809, probe 174834-15710d, fails-without 174835-9a2a47 / 174903-ca5c07, suite 174750-e0b022 pending; CI running |

Drafted (Xapiand:c33b7cc79): `src/database/wide_search.{h,cc}`, `wide_search::Pins` (node-wide FIFO bound on
indexes pinned by wide-search batches, sized to the pool's soft_open) and `wide_search::resolve` (per-name budget
checkpoint). Their call sites are outside the file set: `harness/wide_search_callsites.patch` applies them to
`src/server/search_views.cc` (resolve loop) and `src/database/handler.cc` (`search_batch`: take pins, checkpoint per
shard) for probe jobs only.

Read so far (src = master, build-prof):
- 80ea60 (A2 before, 2,500, no sampling): http0/http1 window max in the 98,304 us bucket (max since start 87.2 /
  91.5 ms), p99 2,048 us, p99.9 2,560 / 3,584 us; search p50 16.1 s (host loaded: the opens' `close()` dominate).
- 928443 (A3 before, 2,000 indexes): 64 clients by names: light p99 0.88 ms, max 1,153 ms, loops max in the
  1.8-2.1 s buckets; by pattern p99 4.58 ms, max 997 ms. T5 bound 2.61 ms passes on p99 (the searches were slow,
  p50 54 s); the max is the miss.
- 9e4e4d / 1d4a33 (`--sample`): macOS `sample` suspends the node ~11.7 s, so a sampled run's lag is invalid; the
  call graph shows the http loops idle in kevent and the work pool in `run_open` -> `close()`. Not enough samples on
  the loops to attribute; switched to the reactor stall watchdog (`restore_stall_reactor_lag.patch`,
  XAPIAND_LAG_SPIN) with `harness/wide_search_stalls.py`: jobs 040712-d69b8d (rebuild), 040719-143d1c (master),
  040720-9c118f (probe patch).

## Next

0. (session 5) Everything read; the lead pushes the head and sends the re-check. Open: the owner's A2-Linux question (Blocked on); A1's attribution job 261118-113058-7cb4e4 ran at 700ffc0c3 (the tool wants a rerun; its evidence is the design doc).
1. Read session-4 jobs: suite, test_node x5, fails-without (3 test_node, 1 probe), half-close probes; A2 macOS (each
   http loop's max <= the control loop's), A2 Linux (<= 1,024 us), A3; F3 from 568ffe (branch misses T5 in 2 of 3 with
   master passing: the change; then the beat-on sampler at 64 clients). Then the lead accepts correctness.
2. (session 3) Read session-3 jobs. A2 Linux: every http loop's max <= 1,024 us after, at 2,500 and 10,000. A2 macOS: each http
   loop's max <= the same run's control loop. Beat A/B: if both arms' p99 fall near 512 us, the branch's p99 rise is
   idle-wake slack; if only master's, slice the branch's short turns and rerun (fails A2 otherwise). A3 from 3774af /
   647326. A4: suite d8889b + CI.
2. Then the correctness review (lead launches).

## Blocked on

A2 on Linux (decision for the owner; the lead takes it): exclusive reruns (session 5) read the branch's http window max at 2.05-3.58 ms and max since start 2.8-7.4 ms (master 60-329 ms), with the guest's idle control loop at <= 2.05-2.56 ms window max / p99 1.8-2.0 ms alone on the host: the bound is below the guest's idle floor at 2,500 and 1.5-1.75x the control's at 10,000. Question for the owner: read A2 on Linux against the control loop (as R6 does for macOS: met at 2,500, 1.5-1.75x at 10,000), or give the VM a finer wake (the bound is not measurable in it as it is). Earlier, strict: the VM runs read the branch's http max at 8.8-19.7 ms (master 162-558 ms) against
the 1,024 us bound, with the idle control loop at 17.7-29.9 ms in the same runs, so the guest under shared strict
holds has a floor above the bound and these jobs cannot show the branch under it. Options: 1. rerun the four VM jobs
as exclusive holds (the host alone; `QUIET_VM` pins the VM's threads), then read the bound; 2. read A2 on Linux as
R6 reads macOS (each http loop's max no worse than the same run's control loop: met, 8.8-19.7 vs 17.7-29.9 ms).
Recommendation: 1 first. Lead took option 1 (the owner takes the Linux reading): resubmitted as exclusive, 10 min each, chained, reusing the guest's builds and indexes (guest binaries are e649ebcb1's; the merge changed only schema.cc): 261119-015840-52e913 (master 2,500), 015843-118f54 (branch 2,500), 015846-7c0eec (master 10,000), 015849-58ebd1 (branch 10,000). Read: the control loop's max alone on the host: 17-30 ms again = the VM's floor (record it); else the branch's http max against 1,024 us.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/wide-search-loop.md` (Attribution; Branch measured; beat A/B); attribution rerun 261118-113058-7cb4e4 |
| A2 | Linux exclusive 261119-015840-52e913 / 015843-118f54 (2,500: http max since start 60.5 / 76.7 -> 6.95 / 2.77 ms), 015846-7c0eec / 015849-58ebd1 (10,000: 329.2 / 299.3 -> 7.38 / 7.39 ms; control alone <= 2.05-2.56 ms window; 1,024 us literally unmet, question in Blocked on); macOS exclusive 214551-71a78d / 214554-8a79f5 (2,500: 105.4 / 75.9 -> 2.08 / 2.05 ms), 214552-695322 / 214555-113315 (10,000: 281.0 / 232.0 -> 2.05 / 2.08 ms; control <= 2.56 ms); strict Linux reads: Linux 214621-f82004 / 214622-52318a (2,500: http max 179.9 / 162.6 ms -> 19.7 / 14.3 ms), 214624-c6c5de / 214625-59b5ea (10,000: 558.1 / 475.8 ms -> 10.4 / 8.8 ms; control loop 17.7-29.9 ms; 1,024 us bound not met, see Blocked on). Pre-fix macOS reads: 10,000 max 322 / 373 ms -> 2.07 ms, 2,500 max 73.5 / 77.8 -> 2.06 / 2.33 ms, control 2,560 us; beat on: branch max 1,011 us. Superseded sets at 3e2edbef4 (204xxx) dropped |
| A3 | session 4: 214557-802978 before (names p99 13.64 / max 1,336 ms, T5 bound 2.32 ms: exit 1), 214558-d15a54 after (1.91 / 2.55 ms, bound 2.27 ms: pass); the earlier branch run 243767 missed (2.85 vs 2.39 ms); F3's A/B 210116-568ffe, exclusive x3: light p99 master 1.51 / 0.68 / 1.09 vs branch 1.86 / 1.58 / 1.57 ms, bound 3.09-3.21 ms, all six pass; max 473-1,982 ms vs 5.3-9.9 ms |
| A4 | session 4: suite 214334-70753d (pass, 44/44 at e649ebcb1; merged head 9b474d811: 261119-015540-f34736 pass, 52/52), test_node x5 214345-e0caec (5/5), probes 214452-f8d1d8 / 214455-d04574; fails without: 214405-d4ad8f (F2), 214416-55f6c4 (F1), 214430-1d4cc0 (F5), 214453-f4a3ac (F1 probe); pending. Earlier, fix cffd0190a: test_node x5 261118-174748-eec809 (5/5), suite 174750-e0b022 (pass), half-close probe vs master 174834-15710d (pass); fails without: probe 174835-9a2a47 (exit 1), test_node 174903-ca5c07 (fails); CI on the head |

## Result

Attribution done: the stall is per-name resolution (each name a resolver-cache miss that decodes its settings
document on the loop, one turn for the whole list); under concurrency the misses contend on the resolver's
`std::mutex`. Mechanism in the set: `src/database/wide_search.{h,cc}` (`wide_search::resolve`, sliced;
`wide_search::Pins`, node-wide FIFO bound on pinned indexes), tests in `tests/test_wide_search.cc`, design in
`docs/design/wide-search-loop.md`. Probe (call sites patched, job 261118-043334-e2a804 against master 043331-286e1f):
SR3 2,500 loops' max 98-229 ms -> 0.6-1.5 ms buckets; S2 64 names light p99 83.8 -> 5.3 ms, max 1,819 -> 26 ms.
Harness: `wide_search_check.py` (`--sample`, creation resume), `wide_search_starve.py` (S2 probe),
`wide_search_stalls.py` + `wide_search_reactor_stalls.patch` (stall attribution).

## Rulings I made

- R7. The lead's assignment names test_node for F1's and F2's handler-level cases, and the fixtures (RunningNode,
  run_child, HoldTurn) live only in `tests/test_node.cc`, outside the file set; the cases are additive there (new
  functions, three table rows, one include). The lead may widen the set or move them.
- R8. F4 (wide-search throughput under contention) is the owner's call per the review; not changed here.

- R1. S2's probe is `harness/wide_search_starve.py` (the review's `wide_starve.py` under the file set's glob), with
  `--create-only/--reuse`; its verdict is T5's bound on the name-list phases: light p99 <= 2x alone p99 + 1 ms.
- R2. A2's "within T4's slice" is read as R23 of x7-gate: every loop's window max at the 1,024 us edge.
- R3. macOS `sample` suspends the node for seconds (11.7-20 s lag in 9e4e4d, 1d4a33, 400442), so sampled runs are
  not SR3 evidence; attribution uses the reactor watchdog instead. 400442 (10,000 before) is replaced by 9d63ea.
- R4. The slot's `.deps/reactor-src` carries `wide_search_reactor_stalls.patch` from 261118-043329-3f59f9 on: inert
  unless `XAPIAND_LAG_SPIN` is set (one getenv at loop start). Evidence jobs after it say so in their `what`.
- R5. Pins apply only to searches wider than one batch: a single-batch search pinning would queue light searches
  behind wide batches; single-batch searches stay bounded by admission.
- R6 (the lead's, Xapiand:86a50e08e; quest Decisions). T4's 1,024 us is read on Linux (VM, strict); on macOS each
  http loop's max must be no worse than the same run's control loop, which reads a 2.0-2.6 ms p99 with no searches.
