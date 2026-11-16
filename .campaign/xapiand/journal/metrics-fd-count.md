# metrics-fd-count: journal

State: waiting on jobs 261116-130822-2391c5 261116-130853-906011 261116-130901-9a8ee3 261116-130908-3189de 261116-130909-79ebc8
Quest: .campaign/xapiand/quests/metrics-fd-count.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

system:a5dc407 (branch quest/xapiand/metrics-fd-count, ~/code/Kronuz/system, worktree system-wt/metrics-fd-count):
/proc/self/fd and /dev/fd listing; system CI green on all 9 jobs (run 43905242900, FreeBSD included). Xapiand:56871ecea
adds tests/test_manager_fd_count.cc and harness/metrics_scrape_{probe.py,build.sh,ab.sh,reactor_lag.patch};
Xapiand:1324fb278 repins system. Jobs: before build 261116-081527-017d42 (-> ~/.scratch/fd-ab/before), after build
261116-083135-f2643d (-> ~/.scratch/fd-ab/after), A2 strict ABBA x3 261116-083148-c3780d (after both), build-test
261116-113507-762c60, e2e 261116-113507-c6d2dc, A1 mutant 261116-082207-28c2d6 (system:a5dc407 with the listing
counting its own dirfd; must fail). docs/mechanisms/metrics.md is drafted with a RESULTS placeholder.

## Next

0. Master merged again (cluster-harness); the A1 mutant and A2 were resubmitted in the new forms (--fails-without, --build). Master merged (no conflict; logger/workpool pins kept, system a5dc407). A2 numbers are in docs/mechanisms/metrics.md
   (69-94 ms to 0.67-1.18 ms; turns over 3 ms 41-67 to 0-36, run beside ~10 light holds). Wait for the verify jobs and CI.

1. When the jobs finish: read A2's log (`campaign job log 261116-083148-c3780d`), put the numbers in
   docs/mechanisms/metrics.md (replace RESULTS) and the Result below; confirm the Xapiand CI on 1324fb278 is green.
2. If A2's loop turn is not under 3 ms, look at what else is in the scrape (the old scan was 52-56 ms of it).
3. The system branch is unlanded: the lead fast-forwards Kronuz/system main to a5dc407 (the pin names that sha).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | test tests/test_manager_fd_count.cc (also Kronuz/system test/test.cc): passing run 261116-130853-906011; build-test 261116-130908-3189de; mutant (fetched system's listing counts its own dirfd, harness/metrics_scrape_fd_mutant.sh, --fails-without) 261116-130822-2391c5 |
| A2 | 261116-130901-9a8ee3 (one checkout: harness/metrics_scrape_build_ab.sh builds before = system 563880a and after = the pin, then ABBA x3 under strict, spin probe over 3 ms); earlier numbers (c3780d, older code) are in docs/mechanisms/metrics.md |
| A3 | e2e 261116-130909-79ebc8 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- The fix is in Kronuz/system (the function's home), one commit, then repinned here; the quest's file set lacks
  CMakeLists.txt and harness/, both touched (pin, test registration, probe).
- FreeBSD keeps the poll scan (its /dev/fd lists only 0-2 without fdescfs; F_MAXFD already bounds the scan to the
  highest open descriptor) rather than linking libutil for kinfo_getfile. The scan now covers the F_MAXFD slot itself
  (F_MAXFD is the highest descriptor, so the old range missed it).
- Listing falls back to the scan when /proc or /dev/fd cannot be opened.
