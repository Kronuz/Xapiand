# field-check-at-map review: correctness

Commit: strata:093bb9651ea0272da196a6ad9a648d02967723c6 (prospect:c99e6bed2854 for the plugin's check)
Verdict: CHANGES
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The check itself is right on every map path I could find and probe. The one blocking item is the
harness the quest lands: `bench/eager_open/ab.sh` still measures whatever binaries sit in OUT.

## Evidence I relied on

- A1 261124-190113-7d240b (field_check_map_test at aaf38cd2) and its mutant 261124-120815-138f97
  (killed): eager, lazy first read, merge input, both damages, and the in-process default vouch.
- A2 261124-193029-58f381: its log carries the build (both sides' commit lines name prospect
  c99e6be); every arm and side gives the same hash on d1000s1/s4/s10. On d1000s4c, proto refuses
  100 of 100 at open (eager, today, warm) or at the query (lazy). Base eager/lazy read it, as expected.
- A3 261124-191203-86c495 and 261124-191208-75ce14: the build lines name base 49fca948, proto
  aaf38cd2, prospect c99e6be on both sides. Every number in `docs/perf/eager-open.md`'s A3 table
  matches the jobs' `ok` lines (for example d1000s10 dirty 59,867 against 59,896 / 59,892, band
  59,851-59,978). Q5's cost claims hold.

## What I checked myself

Probe job 261124-203958-0dda88 (light, fresh checkout, ASan+UBSan -O1, macOS x86_64, strata
093bb965 over prospect c99e6be; exit 0). Scripts are in the strata worktree's
`.scratch/xapiand/review-field-check-at-map-correctness/` (`run.sh`, `probe_strata.cc`,
`probe_prospect.cc`, `CMakeLists.txt`), and the job reruns them as they are.

- field_check_map_test under ASan+UBSan: ok (Q4).
- P1, a wake after demote of a record read from disk: its twin re-maps under the claim, so seg 1 is
  refused and seg 0 reads (Q1).
- P2, heal: it rereads the failed record under `vouch_of(m_)`, reports it damaged, and installs
  nothing unchecked (Q1).
- P3, checkpoint install (eager and lazy target) from a source missing a field: refused with
  `corrupt_segment`. The target keeps its digest, segments, field table and commit. A good source
  installs afterwards with the source's digest (Q1, Q3).
- P4: after a refused eager open and a refused lazy read, restoring the manifest lets a fresh
  eager or lazy open read every segment, with the original digest and no needs_heal. Nothing of
  the refusal is cached (Q3).
- P5, ASan's `__sanitizer_get_current_allocated_bytes` over 4 open cycles: 0 B of growth for a
  refused eager open and for a lazy open with a refused read, the same as a good index. A refusal
  releases its body, mapping and file (Q3).
- prospect crafted segments, each run eager, lazy (every term walked, plus a lookup per table
  field), through a merge, and demoted then read. Variants (Q2, Q4):
  - The dropped field's term pooled (d1000s4c's damage).
  - The dropped field with an own dictionary: a directory entry, confirmed by `entry_of(3)`.
  - A field holding terms in every segment made the key field.
  - A good index with an empty-dictionary segment.

  Each damaged variant is refused at the eager open, at the bad segment's lazy read (4 of 4 for
  the key variant) and after demote. The merge leaves the refused inputs out with needs_heal.
  The empty segment reads everywhere. No sanitizer report.
- Code paths, by reading (Q1). Every body is made in `open_body_of` (index_open.hh:496-523),
  which runs `v.check`. The callers pass the claim they should:
  - `load_files` → `load_segment`/`load_record` pass `vouch_of(m)` (index_open.hh:447-449).
  - `map_body` passes the record's claim (index.hh:1817), and `twin()` copies the claim
    (index.hh:1761).
  - `heal` passes `vouch_of(m_)` (index_open.hh:166); `install` passes `vouch_of(theirs)` for both
    eager and lazy records (index_checkpoint.hh:59,68).

  Only flush (index_flush.hh:20) and merge output (index_merge.hh:209) records keep the default
  vouch; both are written in-process. The other decoders of segment bytes (group/install.hh:161,
  check.hh:64) run `verify_all` and never hand a body to a reader without the index's map.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | `ab.sh row`/`heap`/`identity` run whatever `$out/<side>/eager_open` holds and never compare it with the tree or PROSPECT. The hazard is not stated as a rule either: `docs/perf/eager-open.md` lists only the void jobs. `identity` prints no `sides:` line, so its log cannot show what it ran; this is why e6a947 was void. Three evidence jobs have already been voided this way, and the harness lands with this quest. | bench/eager_open/ab.sh:62-93 (row prints the commit files but does not check them; identity prints nothing); jobs 261124-190110-6a800d, 190112-00d680, 190127-e6a947 | `row`, `heap` and `identity` print the sides line and exit nonzero when `proto/commit`'s first line differs from `git -C $here rev-parse HEAD`, or when the sides' prospect lines differ (or, given PROSPECT, differ from it). Test: a job that runs `row` after rewriting `proto/commit` to another commit must exit nonzero (`--fails-without` on the unguarded script). Add one sentence to eager-open.md's method: rows run in the job that builds them. |
| N1 | no | nit | A refusal at a lazy read or wake is double-prefixed: "strata: corrupt segment: strata: corrupt segment: field id …". `fail(e.what())` rewraps an already prefixed message. | index.hh:1818-1821; probes P1 and the lazy rows of 0dda88; A2 log | Rethrow the original, or strip the prefix when rewrapping. |
| N2 | no | gap | prospect's `check_fields` says "field id outside the field table" for the key-field case too, and names no field id. A1's "naming the field" holds only for strata's test plugin. | prospect fielddict.hh:582-600 at c99e6be; key variant output in 0dda88 | For prospect-check-fields: name the id and say "key field" for the key case. |
| N3 | no | gap | field_check_map_test does not drive a wake after demote of a disk-read record, heal, or install, all paths the doc's table names. P1-P3 show they hold today. | test/field_check_map_test.cc; probe_strata.cc P1-P3 | Fold P1-P3 into the test so a later change to `twin`/`heal`/`install` claims is caught. |
| N4 | no | nit | Neither strata nor prospect at c99e6be indexes a table by a segment's field id: the searcher's statistics are keyed by name, and `add_plan` uses a `std::map`. Here the check refuses foreign ids; it does not guard an out-of-bounds read. The out-of-bounds exposure is in hosts that index `fields()[id]` (Xapiand, drop-open-verify). | prospect searcher.hh:161-175, fielddict.hh:546-560 | None here; worth one line in field-check.md. |
| N5 | no | nit | The map does not check duplicate or unordered directory ids; only `verify_all`'s `check()` does. Nothing indexes by them, and the cursor refuses a field held in both an own dictionary and the pool. | fielddict.hh:428, 604-625 | None. |
| N6 | no | nit | eager-open.md's "Identity (d1000s4 hash 1e688dacf0ef51b0, d1000s10 ee3e4c737f1ce5d0)" are the A3 rows' query hashes. The cited identity job 58f381 prints f21c5648f9f055c0 / da0735ac41071fd0. | docs/perf/eager-open.md (A3 section); 86c495 row.log; 58f381 log | Say "query hash", or cite 58f381's hashes. |

## Not run

- Linux, FreeBSD and TSan: CI's (the ASan+UBSan suite was green at 239d51e4, and this head
  differs by docs and a merge of main).
- build-test 261124-195702-86fcdb was still running when I finished; A4 waits on it.
- Concurrent first maps of a damaged segment from two readers (the CAS in `map_body`): read, not
  probed.
- A manifest damaged in a log entry below the head (`open(..., to)`): not probed.
- Xapiand's consumers of the check.

Session: a subagent of the lead's session, not its own titled session.

## Re-check

Commit: strata:539fa10e (code at 0d39c221)
Verdict: APPROVE
Date: 2026-11-24

Scope: `git diff 093bb965 539fa10e` and the ruling that install mutant 261124-205559-b94bac is
equivalent.

- F1 fixed. `guard` (bench/eager_open/ab.sh) runs before `row`, `heap` and `identity`. It prints
  the sides line (which `identity` now has too) and exits 3 before any run in three cases: proto's
  strata differs from the tree's HEAD, base or aa's strata differs from `$JOB_REF_BASE`, or a
  side's prospect differs from `$JOB_REF_PROSPECT`. When that is unset, the sides' prospects must
  agree with each other. `exit` inside the function ends the script.
  - Evidence: test_guard.sh covers each refusal on all three commands (passed 2d1c17), and the
    unguarded script fails it (8c5e65).
- N1 fixed. `fail(..., false)` is called from inside the `corrupt_segment` handler, where `throw;`
  rethrows the handled exception; a `corrupt_index` is still rewrapped. The single-prefix CHECK
  fails without the fix (16b399).
- N3 fixed: wake, heal and install cases match my P1-P3. The twin and heal mutants are killed
  (96bca6, 765a8d).
- N6 doc: fixed.
- The equivalent-mutant ruling stands. The own copies (index_checkpoint.hh:59, 68) are
  `write_new` of the bytes `load_files(whole=true)` just checked under the same `vouch_of(theirs)`.
  A later map of a copy also passes size, footer crc and identity. A different field id there
  therefore needs section bytes rewritten under intact block and term checksums: a forgery, which
  the trusted-cluster model excludes. Under honest faults the claim on the copies is
  unobservable. The copies keep it, and field-check.md now says the test rests on the load-side
  check.
- The crash-kill is acceptable. 7f00f1's log shows CHECK failures at field_check_map_test.cc:313
  and :314 (no refusal; digest, segments and fields changed) before the abort. The abort is the
  unguarded `ix.digest()` then mapping the damaged copies, and the mutant causes both.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | nit | With `JOB_REF_BASE` unset, the guard checks base and aa only for prospect agreement, not their strata commit or each other. A row run without `--ref BASE` can still measure a stale base. | bench/eager_open/ab.sh guard (`want_base` empty) | Require `JOB_REF_BASE` for `row`/`heap`, or check that base and aa have equal stamps. |
