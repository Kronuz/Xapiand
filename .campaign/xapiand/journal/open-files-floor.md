# open-files-floor: journal

State: blocked on lead: Kronuz/system leaks two descriptors on Linux; CI 44966941683 fails test_coverage_main at the minimum
Quest: .campaign/xapiand/quests/open-files-floor.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged master at a4d674b00346 (clean). CI at 4a07d2e (run 44966941683) fails test_coverage_main on Linux in all three
jobs (Build + unit + smoke, TSan, ASan + UBSan): this quest's check at tests/test_coverage_main.cc:529, the start at
the exact minimum, gets SystemExit(72). Master's CI (45007834306) is green; master has no such check. Not a flake.

Cause: Kronuz/system at a5dc407 (Xapiand's pin), system.cc, Linux branches of get_open_files_system_wide and
get_max_files_system_wide each io::open("/proc/sys/fs/file-nr") and never close it. adjustOpenFilesLimit calls both
before get_open_files_per_proc, so the test measures 3 open (minimum 78) and the function measures 5 (minimum 80):
78 < 80 is refused. macOS uses sysctl, so local jobs pass. A production start on Linux also keeps the two descriptors.

## Next

1. After the system fix and pin bump reach master: `campaign land open-files-floor --merge-main`, rerun A1 (unit,
   coverage e2e, fails-without) and A2 at the merged commit, push, read CI.

## Blocked on

Lead: the fix is in Kronuz/system and Xapiand's CMakeLists.txt pin, outside this quest's file set.
1. Recommended: close fd in both Linux readers of system.cc (one io::close after the read, and on the read-error
   path), land in Kronuz/system, bump the GIT_TAG at CMakeLists.txt:633 on master. Fixes the leak, not the symptom.
2. Not recommended: have the test call the two system-wide readers before measuring the open count; it hides a
   real descriptor leak.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261121-163011-1ee7dc, 261121-204931-2e7d5a; fails without: 261121-163015-5d9da9 (log: complete build, 3 of 97 checks fail at tests/test_coverage_main.cc:525, 543, 575) |
| A2 | 261121-163007-0100b9 (the suite check, 80/80 at d9f7622; macOS only) |

Examined: 261121-163011-1ee7dc (unit + coverage, 97 checks pass, macOS); 261121-204931-2e7d5a (main e2e, 91 checks,
0 failed); 261121-163015-5d9da9 (master's main.cc, fails as it must); 261121-163007-0100b9 (suite passes);
261121-163013-2b7cd3 (timed out at 30 min mid-build, invalid; replaced by 261121-204931-2e7d5a). CI 44966941683 at
4a07d2e: failed, Linux only, see Doing. All at pre-merge commits: rerun at the landed code once unblocked.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
