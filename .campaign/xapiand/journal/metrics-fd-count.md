# metrics-fd-count: journal

State: working
Quest: .campaign/xapiand/quests/metrics-fd-count.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

system:a5dc407 (branch quest/xapiand/metrics-fd-count in ~/code/Kronuz/system, worktree system-wt/metrics-fd-count) lists
/proc/self/fd and /dev/fd instead of polling; pushed, CI running. Xapiand:56871ecea adds tests/test_manager_fd_count.cc
(registered in CMakeLists.txt) and the probe harness/metrics_scrape_*. Before-binary build job 261116-081527-017d42
(-> /Users/kronuz/.scratch/fd-ab/before).

## Next

1. system CI green -> repin CMakeLists.txt system GIT_TAG to a5dc407, push, CI.
2. Build the after binary (metrics_scrape_build.sh) at the repin commit; A2 job: `quiet strict`
   harness/metrics_scrape_ab.sh before after 3.
3. A1 `build-test` check, A3 `e2e` check via `campaign verify`; docs/mechanisms/metrics.md with the numbers.

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

- The fix is in Kronuz/system (the function's home), one commit, then repinned here; the quest's file set lacks
  CMakeLists.txt and harness/, both touched (pin, test registration, probe).
- FreeBSD keeps the poll scan (its /dev/fd lists only 0-2 without fdescfs; F_MAXFD already bounds the scan to the
  highest open descriptor) rather than linking libutil for kinfo_getfile. The scan now covers the F_MAXFD slot itself
  (F_MAXFD is the highest descriptor, so the old range missed it).
- Listing falls back to the scan when /proc or /dev/fd cannot be opened.
