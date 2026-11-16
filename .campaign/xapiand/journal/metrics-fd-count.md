# metrics-fd-count: journal

State: waiting on jobs 261116-081527-017d42 261116-083135-f2643d 261116-083148-c3780d 261116-083142-1bc561 261116-083142-e4f744 261116-082207-28c2d6
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
261116-083142-1bc561, e2e 261116-083142-e4f744, A1 mutant 261116-082207-28c2d6 (system:a5dc407 with the listing
counting its own dirfd; must fail). docs/mechanisms/metrics.md is drafted with a RESULTS placeholder.

## Next

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
| A1 | test tests/test_manager_fd_count.cc (and system test/test.cc): build-test 261116-083142-1bc561; mutant (listing counts its own dirfd) 261116-082207-28c2d6 |
| A2 | 261116-083148-c3780d (strict ABBA x3, spin probe over 3 ms; builds 261116-081527-017d42 before, 261116-083135-f2643d after) |
| A3 | e2e 261116-083142-e4f744 |

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
