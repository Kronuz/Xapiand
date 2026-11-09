# Hunt list

Exploration targets for the next batch: review follow-ups, non-blocking findings, benchmark losing
rows, small bugs. One row per target with its evidence and where to start; a row leaves when its
quest starts (name it) or is dropped with a reason.

| Target | Evidence | Start in | Repo | Status |
|---|---|---|---|---|
| catalog_s8_smoke under TSan takes 41.6 min of linux-tsan's 60-min job (one rerun passed at 58 min) | strata CI at the wbuf-inline head | test/catalog_s8_smoke*, .github/workflows | strata | waits for manifest-log's file set |
| wbuf-inline review nits N1-N4 | strata `docs/reviews/wbuf-inline-correctness.md`; Xapiand `reviews/wbuf-inline-correctness.md` | the review's file:line | strata | waits for manifest-log's file set |
| x-pool review C3, C5, C6 (TTL lower bound; node-stop log match; re-pin counted as a use) | `reviews/x-pool-correctness.md` C3, C5, C6 | tests/test_pool.cc:2223, tests/test_node.cc, src/database/pool.cc:690 | Xapiand | quest x-pool-followups (ready, landing) |
| `harness/restore_stream_check.sh` should wait for the restore instead of racing it | x-pool journal; probes in `.scratch/x-pool-probes/repeat_restore.sh` | harness/restore_stream_check.sh | Xapiand | quest x-pool-followups (ready, landing) |
| e2e-harness newman pin (newman 6 found by PATH order, not pinned) | `journal/e2e-harness.md` A3, harness/engine_e2e.sh:25 | harness/engine_e2e.sh, harness/run_all.sh | Xapiand | open |
| (tool) A docs-only commit voids every job's evidence (identity is the whole tree minus `.campaign/`): sortable-proof's README/AGENTS edit reran a 60-min fuzz hour, the exhaustive run, preflight and build-test | sortable-proof journal (Xapiand-wt:ab713b8cf), jobs 261109-145953..56 | agent-campaign scripts/campaign.py code_tree | agent-campaign | open: let a job declare the paths it reads, or exclude docs/** and *.md unless the command reads them |
| `src/main.cc:663` and the changelog say "alpha.9 or earlier" refuses; alpha.10 data is Xapian too | docs-upgrade-page journal, Blocked on 3 | src/main.cc:663, CHANGELOG.md | Xapiand | open |
| `harness/restore_stream_check.sh` detects the restore's end by 10 s of an unchanged count (a heuristic); an exact signal needs a "restore ended" log line | x-pool-followups journal, Rulings | src/server/search_views.cc, harness/restore_stream_check.sh | Xapiand | open |
