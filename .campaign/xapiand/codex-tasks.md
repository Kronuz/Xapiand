# Tasks for Codex (2026-11-13 to 11-16)

The xapiand lead reads each deliverable after the quota reset (11-16 05:00 UTC) and lands or
plans from it. Each task is self-contained, ends in a named report or branch, and stays out of the
file sets of the active quests (x7-gate, exact-id-keys, and sortable-encoding once it starts).

## Rules for every task

- Builds, tests and fuzzing run under `quiet light NAME MINUTES "what" -- cmd` with at most 4 jobs;
  every timing under `quiet strict` (or `quiet exclusive` for numbers that decide), at most 10
  minutes per run. Run `quiet check` first. Detent's agents and the xapiand campaign share the host.
- Never touch: `src/database/*`, `src/serialise.*`, `src/query_dsl.cc`, `src/engine/strata/*`,
  `docs/gates/*`, `.campaign/**` (except this file's Status table), Kronuz/sortable's byte
  format (`vectors.txt`, `reject.txt` and anything that changes them).
- Never open GPL sources: `src/sortable_serialise.{cc,h}`,
  `contrib/python/xapiand-py/xapiand/utils/sortable.py`, or Xapian.
- No AWS or any paid resource. Leave the qemu VMs on ports 18762/18763 (Detent's) alone.
- One branch per task, named `codex/<task>`, pushed to origin; never push to `master`/`main`
  except a quick fix (below). Use `git -C DIR`, not `cd DIR && git`.
- Commit as `Germán Méndez Bravo <german.mb@gmail.com>`; no AI attribution or co-author lines.
- Quick fix: an obvious cause, tens of lines outside formats, durability, concurrency and security,
  with a test that fails without it, may go straight to `master`/`main` with CI green after; say so
  in the report.
- Numbers: before and after at named commits, the A/A spread, honest magnitude (a loss or "no
  change" is a result). Name every command so the lead can rerun it.
- American spelling in docs and new names (serialize, behavior).

## C1. Kronuz/sortable: encode and decode speed

Why: owner ruling D87 makes sortable-encoding land only if the variable codec is as fast as the old
value codec. Nobody has optimized it yet.

- Repo `~/code/Kronuz/sortable` (`sortable.hh`, `test/`, `tools/model.py`, `fuzz/`).
- Add a microbenchmark (`bench/` with a CMake target) of `encode` and `decode` for int64, uint64
  and double on (a) the real-value sets `docs/design/sortable-encoding/sizes.py` produces (branch
  `design/sortable-encoding` in Xapiand; its `sizes.txt` lists the sets) and (b) uniform random
  values per width. Report ns per value and bytes per value.
- The bar: the old codec's per-value cost, measured as a black box through Xapiand's public
  `Serialise::serialise(double)`/`Unserialise::...` entry points (call them from a tiny program
  linked against a Xapiand build); never read its source.
- Then make sortable faster without changing a single output byte: the existing tests,
  `vectors.txt` identity, the property test and the fuzz corpus must pass unchanged. Keep each
  change only if it wins outside the A/A band on both x86_64 macOS and the Linux build.
- Deliverable: branch `codex/sortable-speed`; report `sortable/docs/perf/speed.md` with the table
  (old codec vs sortable before vs after, per type and value set, ns and bytes), the kept and
  dropped experiments, and commands.

## C2. Xapiand: what wakes an idle node

Why: idle master uses 0.48% of a core and about 37 wakeups a second (hunt.md row, 11-13). With
millions of indexes or many idle nodes per host that is power for nothing; an idle node should
sleep until scheduled work.

- Build master (`cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF`), run
  `bin/xapiand -D <dir> --port 18899` with one index holding one document, idle 5 minutes.
- Attribute every wakeup to its thread and the timer or loop that fires it (macOS: `sample`,
  `dtrace -n 'mach_kernel::wakeup'`-style probes or `powermetrics --show-process-wakeups`; Linux VM
  if available: `perf sched`/`perf trace`). Thread names come from `--log thread-names`.
- Deliverable: branch `codex/idle-wakeups` with only `docs/perf/idle-wakeups.md`: a table of
  thread, source (file:line), period, wakeups per second, and a proposed fix per source. Don't
  implement fixes in the timer, wheel or housekeeping code: that work is promised to Detent's
  campaign; the lead turns the note into quests.

## C3. FreeBSD CI for Xapiand

Why: FreeBSD is a stated miss at Phase 7c (owner D86) for lack of a CI workflow.
`.github/workflows/freebsd.yml` today only packages tags.

- Add a job (push to `master` and `quest/**`, `quick/**`, `codex/**`; skip `.campaign/**`-only
  pushes) that builds Release with tests in the vmactions FreeBSD 14.x VM and runs `ctest`,
  plus the docs E2E if it fits the runner's time limit. Keep the tag packaging job as it is.
- Fix what fails on FreeBSD only where the fix is small and portable; list everything else.
- Deliverable: branch `codex/freebsd-ci` with the workflow and fixes, CI run links, and
  `docs/dev/freebsd.md`: what passes, what fails (test name, error), and the cause where known.

## C4. Fuzz the cpp-libraries parsers that have no fuzzer

Why: the prospect decoders got four CPU-hours with zero findings; these parsers of untrusted input
never had any.

- Repos under `~/code/Kronuz/`: `http-parser`, `base-x`, `cuuid` (decoding), `datetime` (parsing),
  `boolean-parser`, `strict-stox`, `radix-router`. Follow the shape of an existing one
  (`url-parser/fuzz`, `msgpack/fuzz`): a libFuzzer target with ASan and UBSan, a seed corpus.
- One CPU-hour per target under `quiet light`. Every crash becomes a minimized reproducer, a
  regression test that fails without the fix, and a fix (quick-fix rule) on `main`.
- Deliverable: per repo, the `fuzz/` target on `main` (CI green), and one report
  `~/code/Kronuz/cpp-libraries/docs/fuzz-2026-11.md`: target, CPU time, executions, coverage
  edges, findings with commits. Add each generic finding to
  `~/code/Kronuz/completion/docs/PLATFORM_FINDINGS.md`.

## C5. Xapiand builds: the last ccache misses

Why: after Xapiand:05960cb81, 31 of 202 translation units still miss ccache's direct mode in a new
checkout, and the cache sits at its 15 GB cap with 5,312 cleanups.

- Find what differs per checkout for those 31 (`CCACHE_DEBUG=1`, compare the `.ccache-input-text`
  of two checkouts): absolute paths in generated headers, `__FILE__`, `-I` of build dirs.
- Fix them in CMake (quick-fix rule) and measure two fresh checkouts: direct hits out of total.
- Measure the working set: total cache bytes one full build of each configuration uses (Release,
  ASan, TSan, test targets), and recommend `max_size`.
- Deliverable: the fix on `master` (CI green) or branch `codex/ccache`, and
  `docs/dev/ccache.md`: before/after direct hits, the working set per configuration, the
  recommended size.

## C6. Python client: a clean-room port of the sortable codec

Why: sortable-encoding needs the Python client to produce the same bytes as C++ (its criterion
A3), with names that read well (D87).

- Work only from `~/code/Kronuz/sortable` (`sortable.hh`, `tools/model.py`, `docs/`), never from
  the client's existing GPL module (`contrib/python/xapiand-py/xapiand/utils/sortable.py`): do not
  open it.
- New module `contrib/python/xapiand-py/xapiand/sortable_codec.py` with `encode(value)` and
  `decode(data, kind)` for int, float (and bool if the C++ codec has it); typed (Pyrefly strict),
  Ruff clean.
- Identity: every line of `sortable/test/vectors.txt` (113,491) encodes and decodes the same, plus
  10^6 seeded random values against `tools/cli.cc` output; `reject.txt` inputs are rejected.
  Speed: values per second for encode and decode.
- Deliverable: branch `codex/py-sortable` with the module and tests; a short
  `contrib/python/xapiand-py/docs/sortable-codec.md` with the identity and speed results. It lands
  inside sortable-encoding, not on its own.

## C7-C9. Detent (from Detent's lead, 11-13)

Each on its own branch `codex/<name>` from `main` in the named repo, committed and pushed to that
branch, never to `main`, no campaign-state edits. Evidence: commands and numbers in the report.

### C7. codex/http-idle-baseline (repo Kronuz/http)

- Measure bytes per idle connection, plain and TLS, at 10K and 100K idle connections, with an
  owner breakdown: socket, OpenSSL SSL/BIO buffers, read/write buffers, parser, coroutine frames,
  timers. Allocator stats plus RSS, release build, `quiet exclusive`.
- Report `http/docs/perf/idle-cost-baseline.md`: commands, raw numbers, top owners, and 2-3
  candidate cuts with estimated bytes saved.
- Host limit (lead, 11-13 19:10): the first 100K runs left ~30,000 loopback sockets in
  FIN_WAIT_1/CLOSING to ports 18920-18926 and exhausted macOS's 16,384 ephemeral ports, which broke
  other campaigns' jobs (EADDRNOTAVAIL) and GitHub access. Spread clients over source addresses
  (127.0.0.2, 127.0.0.3, ... bound before connect), close server and client sides cleanly, check
  `netstat -an -p tcp | grep -c FIN_WAIT_1` is back near zero after each run, or run the 100K
  arms in the Linux VM.
- Add only a bench under `bench/`; never touch a product header (`*.h`). Feeds Detent quest
  http-idle-cost (A1).

### C8. codex/ci-flakes-triage (repo Kronuz/Detent)

- Reproduce the six flakes listed in `.campaign/detent/quests/ci-test-flakes.md` with repeat runs
  under load (`quiet light`, `ctest --repeat until-fail`). For each: root cause with evidence (logs,
  a minimal repro) and a proposed fix.
- Report `docs/dev/ci-flakes-triage.md`. Never touch `src/`, `include/` or `.campaign/`; test
  changes only as proposals in the report.

### C9. codex/s6-v1-removal-survey (repo Kronuz/Detent)

- Survey for quest namespace-s6-v1-removal (`.campaign/detent/quests/namespace-s6-v1-removal.md`,
  Detent D93): every user of the v1 wire, the old Cursor, poll mode and AuthorizedScope by file and
  line; and the cost of the open question (the agent keeps one scope-named table entry, Detent D88,
  against one entry per followed namespace, Detent D8): files and lines touched, on-disk and format
  effects, risks, a recommended order of commits.
- Report `docs/design/s6-v1-removal-survey.md`. Read-only otherwise: never touch `src/`,
  `include/`, `tests/` or `.campaign/`.

Not for Codex now (Detent's call): namespace-followups, consensus-late-append.

## Status (Codex fills one row per task as it finishes or stops)

Commit this file on Xapiand `master` after each row (a `.campaign/`-only commit runs no CI). If
pushing is blocked, say so in the row and leave the branch local.

| Task | State (done, partial, blocked) | Repo and branch | Commit | Report path | Result in one line |
|---|---|---|---|---|---|
