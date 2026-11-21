# xapiand-final-release: journal

State: blocked on lead: the tool reads this release line as a master feature branch (base, file set, A5's evidence kind); A1-A3 rerun at head queued. A6 waits on the owner.
Quest: .campaign/xapiand/quests/xapiand-final-release.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Backports on the quest branch (based on the `xapiand` tip e692a64b1, so the tool's worktree base fits). Done: parser cherry-picks (4b9ee717e, 8ab6b2b26) with the adapter on by default and a nested NEAR test; id 0 (971f90559, d0c9a5733) with the harness check added to the smoke layer; A3 (fdb32df96); changelog (72ebbfa83). Job 261120-155714-a3ba72 failed (getStopWords include resolved to the wrong stopper.h); fixed; build-test 261120-172811-fc88f8 failed 3 of 61 checks of test_generate_terms (lines 334-336, the positive/floating terms-only loop); base-commit run (e692a64b1) is job 261120-174741-3e4b95 to tell pre-existing from mine.

CI on `xapiand` at bb04f6d63 failed twice (lead's log ~/.scratch/xapiand/lead/xapiand-branch-ci.log):
1. test_text_parser.cc's `#include "xapian.h"` resolved only through Homebrew's /usr/local/include (Xapian 2.1.0) locally; Linux has none. Fix 5c4c0ce2d: src/engine/xapian (the vendored umbrella) first on that target's include path. Guard job 261120-202710-3443a6 (dep file names the vendored header, never /usr/local/include); 261120-202717-edd5aa is the same check failing at bb04f6d63.
2. smoke-id0 under ASan/TSan (222 and 452 of 999 kept). Cause: Schema::index tested `!document_id`, and MsgPack's bool of 0 is false, so a body's `"_id": 0` counted as no id (the URL-id refusal in get_prefixed_term_id never ran on RESTORE). On a new index doc 0 then set the id type to uuid while the others prepared beside it; when the numeric type won instead, id 0 became the new-id request and id 1 overwrote it, which is why the plain macOS run passed (job 261120-202217-a29843). Fix 91d4c3e62: any number is an explicit id; numeric id 0 throws ClientError. The harness now also requires RESTORE's item 0 to be 400 and `indexed` 999, and ctest runs it (5c4c0ce2d), so the local build-test catches it. Fails-without: 261120-202653-9ebd4f at 5c4c0ce2d; build-test at the fix: 261120-202700-a1e84b.

No-publish dry runs at 30815d91f (Release 44871494230, FreeBSD 44871496282; CI green) failed on two causes, both fixed in df3ff48b8:
1. Docker/RPM (gcc, no system lz4): storage@671eaacb declares compressors at ed590c8 before Xapiand's own declaration, and FetchContent keeps the first, so 27191e0ff's pin of 816bc82 (9d2a87c using-declarations) never applied. compressors fetches lz4 1.10.0, hashes.hh inlines its xxhash.c, and ed590c8's `using namespace` makes PRIME32_* ambiguous. Fix: declare compressors before storage (master does the same, line 458).
2. FreeBSD (clang -Werror): config.h.in `#define XXH_STATIC_LINKING_ONLY 1` vs /usr/local/include/xxhash.h's empty definition. Fix: empty define, master's b078d5a18 line.
3. Bottles (run 44876492170): GitHub retired macos-14; a4ca8aedf moves the arm64 matrix to macos-15/macos-26 (GA labels per actions/runner-images README) and the Rosetta cell to macos-15 (bottle targets Sequoia). No other branch workflow names macos-14; master's bottles.yml still does (lead's).
VM jobs (~/.scratch/xapiand/final-release-build/vmbuild.sh: Linux VM 18782 runs the Dockerfile builder stage in docker.io; FreeBSD VM 18784 runs freebsd.yml's steps plus pkg xxhash): fails-without at 30815d91f 261120-225040-bec8a6 (linux), 261120-224651-baca46 (freebsd); at df3ff48b8 261120-225042-599237 (linux), 261120-224652-8177ce (freebsd).

Results: fails-without at 30815d91f reproduce both causes: 261120-225040-bec8a6 (gcc Docker, PRIME32 ambiguous), 261120-224651-baca46 (FreeBSD, XXH macro redefined). At df3ff48b8 the Docker builder stage passes: 261120-225042-599237 (gcc-13, full build and install; -Wodr warnings on compressor_deflate.h only). FreeBSD 261120-224652-8177ce failed past the XXH fix on rapidjson v1.1.0's GenericStringRef::operator= (document.h:319), which clang 19 (the VM's base) rejects and clang 18 (the workflow's FreeBSD 14.2 image, per the dry-run log) accepts; master pins rapidjson 24b5e7a. Rerun with pkg llvm18: 261120-231338-2550ab. Pre-existing on the branch, not this quest's change: risk for the lead (FreeBSD 14.3+/clang 19 source builds).

## Next

1. Jobs at Xapiand:68a091f38 (code = Xapiand:cfc2a5b80): build-test with -G Ninja for A1-A3 261121-145254-58eeb6 (the gate's own command has no -G Ninja; this run uses it, build dir build-ninja); A5 CI-run check 261121-145240-4ff3db (level none). Cite them in Evidence when done.
2. Lead: the base and file set under Blocked on; push nothing new to `xapiand` (it is at Xapiand:cfc2a5b80 already).
3. Owner: A6, approve the tag v1.0.0-alpha.11 at Xapiand:cfc2a5b80. No tag from agents.

## Blocked on

Lead (2026-11-21, resumed): shape. This is a release line, not a master feature branch. The quest's Outcome cuts the release "from the official `xapiand` branch (created from v1.0.0-alpha.10, Xapiand:3fe13a2fb)" and its Decisions put Xapian-only fixes on `xapiand`, "merged into master too", with landing by `--record` after the tag. The quest branch starts at the `xapiand` tip Xapiand:e692a64b1 and `xapiand` is already at Xapiand:cfc2a5b80 (head minus journal commits). Merging master (strata, 1,502 files) would put the engine rewrite into the Xapian-only release and break the contract (alpha.10's HTTP API and formats). I did not merge.

What the tool needs from the lead (options; recommendation 1):
1. Recommended: give the quest `xapiand` as its base (or record it now as a release-line quest) so `master changed 1502 files` and the out-of-set diff are measured from Xapiand:e692a64b1, not master's merge-base Xapiand:bedae4b68; set its file set to the paths below; land with `campaign land xapiand-final-release --record cfc2a5b80 --reason ...` once the owner tags.
2. Leave the tool's gaps as known-false and record the landing by hand; the three gaps then stay on `campaign status` until then.

Out-of-set paths, against the `xapiand` base Xapiand:e692a64b1 (the placeholder set `tests/test_xapiand_final_release*`, `docs/release/xapiand-final*` matches no file; every change is below):
- .github/actions/build-bottle/action.yml, .github/workflows/freebsd.yml, .github/workflows/release.yml: Xapiand:1114106db, `publish`/`ref` dispatch inputs so A5's Release/FreeBSD/Bottles dry runs build a sha without publishing.
- .github/workflows/bottles.yml: Xapiand:1114106db (same) and Xapiand:a4ca8aedf (GitHub retired macos-14; arm64 on macos-15/macos-26).
- CMakeLists.txt: A1 adapter and A/B build (Xapiand:4b9ee717e, 8ab6b2b26, 0463bc916); vendored xapian.h first for test_text_parser and the id-0 check in ctest (Xapiand:5c4c0ce2d); version 1.0.0-alpha.11 (Xapiand:fc7142e3a); compressors declared before storage for the gcc Docker build (Xapiand:df3ff48b8); RapidJSON pinned at master's commit for clang 19 (Xapiand:d2fa3e15d).
- config.h.in: empty XXH_STATIC_LINKING_ONLY define, FreeBSD clang -Werror (Xapiand:df3ff48b8).
- docs/src/content/docs/changelog.md: A4 and the alpha.11 notes (Xapiand:72ebbfa83, fc7142e3a, cfc2a5b80).
- harness/id_zero_check.py, harness/run_all.sh: A2's check (Xapiand:d0c9a5733, 52cbbaab3, 5c4c0ce2d).
- src/database/handler.cc, src/database/schema.cc: A2 refusal of numeric id 0 (Xapiand:971f90559, 91d4c3e62).
- src/engine/xapian/query.cc, src/engine/xapian/text_parser.cc, src/engine/xapian/text_parser.h, tests/test_text_parser.cc: A1 queryparser adapter and its test (Xapiand:4b9ee717e, 8ab6b2b26, 0463bc916, 2060debe2).
- src/multivalue/range.cc, src/serialise.cc, tests/test_generate_terms.cc: A3 (Xapiand:fdb32df96, ad037e7b1).
- Not this quest's: docs/dev/agg-blowup.md, harness/agg_buckets_check.py, src/aggregations/{aggregations.cc,aggregations.h,bucket.h}, tests/test_engine_sort.cc and part of CMakeLists.txt are on `xapiand` before the quest (Xapiand:387efb944, 0f9e9e590 and the pins, Xapiand:bedae4b68..e692a64b1); the tool lists them only because it diffs from master's merge-base.

Evidence (problem 3): A1-A3's build-test ran at Xapiand:91d4c3e62 and Xapiand:d2fa3e15d; the code to tag (Xapiand:cfc2a5b80) differs from d2fa3e15d by the changelog only. Rerun at head queued (Next). A5's evidence is GitHub's, not a local job: CI, Release, FreeBSD and Bottles all `success` at Xapiand:cfc2a5b80 on `xapiand`; a job reading those four runs and checking their sha's tree equals head's (journal aside) is queued so the tool can count it. The VM jobs (Docker, FreeBSD) were diagnostics for the build fixes and need no rerun.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | build-test 261120-202700-a1e84b at 91d4c3e62 (23/23, test_text_parser); include guard 261120-202710-3443a6, failing at bb04f6d63 261120-202717-edd5aa (its dep file names /usr/local/include/xapian.h) |
| A2 | build-test 261120-202700-a1e84b (id_zero_check in ctest, 9.3 s); fails-without 261120-202653-9ebd4f at 5c4c0ce2d: "RESTORE answered 200 with item 0 {"_id": 43}", "indexed 1000" |
| A3 | build-test 261120-202700-a1e84b |
| A4 | `docs/src/content/docs/changelog.md`: "Some numbers are stored in a lossy sortable form." |
| A5 | 261121-145240-4ff3db at 68a091f38: CI 44895581156, Release 44895581852, FreeBSD 44895584092, Bottles 44895586318 all success on `xapiand` at cfc2a5b80, whose tree is head's minus .campaign (the Bottles Intel Rosetta cell fails, best effort, run still success) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- A5 no-publish: `ref` applies only when publish=false (a release always builds from its tag); step guards use `env.PUBLISH`, the docker-manifest job a job-level `if`. The bottle tap merge still runs locally in a no-publish run (sha archive url plus explicit version) and only the commit/push step is guarded; this proves the merge before tagging.

- Tool flow: the quest branch already sits on `xapiand` (worktree base fits); `campaign land` lands on main, so landing is `--record` after the tag, as the quest says. `campaign brief` flags "master changed 1485 files" and out-of-file-set changes: expected, this branch is not master's.
- A1: the adapter was opt-in (XAPIAND_QUERYPARSER=1); made default, =0 selects Xapian. test_text_parser pins =0 as its reference.
- A2: the id-0 check was a harness script only; run_all.sh smoke runs it, and ctest too (5c4c0ce2d): the local build-test is ctest only and never ran smoke.
- A2 fix lives in src/database/schema.cc, outside the listed file set: the lead assigned the restore-path fix. Explicit `_id` of false, null or "" still asks for a new id (unchanged); only numbers became explicit, so `"_id": 0` on a uuid-id index is now a cast error instead of a random uuid.
- A3 float bounds: lower bound ceil, upper bound floor, saturating, in range.cc integer_bound.
- test_generate_terms failure (job 261120-172811-fc88f8) was mine: a non-negative int can carry the NEGATIVE_INTEGER tag, so integer_bound emptied positive ranges; passes at base (job 261120-174741-3e4b95). Fixed by reading the sign from the value, regression checks added; build-test 261120-181401-b5cc95.
- A5 build fixes: config.h.in is outside the file set; the lead assigned the FreeBSD build fix there. The FreeBSD VM job installs pkg xxhash beyond freebsd.yml's list because the GitHub FreeBSD image has /usr/local/include/xxhash.h (the dry-run's note names it).
