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
| (tool) A docs-only commit voids every job's evidence (identity is the whole tree minus `.campaign/`): sortable-proof's README/AGENTS edit reran a 60-min fuzz hour, the exhaustive run, preflight and build-test | sortable-proof journal (Xapiand-wt:ab713b8cf), jobs 261109-145953..56 | agent-campaign scripts/campaign.py code_tree | agent-campaign | fixed: agent-campaign:6579ca6 |
| `src/main.cc:663` and the changelog say "alpha.9 or earlier" refuses; alpha.10 data is Xapian too | docs-upgrade-page journal, Blocked on 3 | src/main.cc:663, CHANGELOG.md | Xapiand | open |
| `harness/restore_stream_check.sh` detects the restore's end by 10 s of an unchanged count (a heuristic); an exact signal needs a "restore ended" log line | x-pool-followups journal, Rulings | src/server/search_views.cc, harness/restore_stream_check.sh | Xapiand | open |
| (tool) A long light job (docs-upgrade-page A2, --minutes 240) holds an overdue drain for hours: perf-watch-peek's strict screen and exclusive confirm (15 min of timing) waited from 14:57 past 16:30, with light holds declared until 19:45 | jobs 261109-144804-ff5f5b, 261109-150350-f39012, 261109-153551-16cd29 | agent-campaign scripts/campaign.py cmd_worker (drain reservation) | agent-campaign | open: split long light jobs, or let a short drain run on cores a long light job does not use (macOS has no pinning) |
| sortable-proof review S1: Ruling 5 and docs/mechanisms/sortable.md say a hand-built exponent can overflow `to_double`; it cannot | reviews/sortable-proof-security.md S1 | docs/mechanisms/sortable.md | sortable | open |
| sortable-proof review S3: the fuzz canonicality check runs on the whole input only, and every seed starts with a split byte | reviews/sortable-proof-security.md S3 | fuzz/decode_fuzz.cc, fuzz seeds | sortable | open |
| sortable-proof review S4: the CLI accepts leading zeros, `i -0` and uppercase hex; document or tighten | reviews/sortable-proof-security.md S4 | tools/cli.cc, docs | sortable | open |
