# xapiand: plan

The owner writes Goals, Non-goals and Reserved decisions; the lead keeps the rest current. Quest
state is in `state.json` (`campaign status`, the quest log); this file holds what the state cannot.
The roadmap is strata's `docs/PLAN.md` from its first version (strata:665677b1, 2026-10-11): Phases
0, 0.5, 1, 2, 3, Tracks C, P and D, 3R, 4, the `document` profile gate, 5, 6, 7 (7a, 7b, 7c), 8 and
the recovery tooling, in that order. Xapiand's `docs/status/road-to-release.md` (CURRENT 2026-11-01,
carried conditions to 2026-11-03) details 7c, Phase 8 and the strata lane, with the rulings in
`docs/status/xapiand-on-strata.md`, `docs/design/xapiand-on-strata.md` and `docs/design/phase7c.md`
section 6. The owner confirms Goals, Non-goals and Reserved decisions.

## Goals

- The first Xapian-free release of Xapiand: Phase 8 at `rf = 1`, single node, shipped after the
  single-node gate (7a and 7c) (road-to-release header; strata `docs/PLAN.md` Phase 7, owner
  2026-10-29).
- Phase 7c's gate passes on master: rows T1 to T13 of `docs/design/phase7c.md` section 4, reported
  in `docs/gates/phase7c.md`, with the Linux and FreeBSD runs owed since 7a (C5) (road-to-release, X7).
- Before the release (D39, D40): the rows of Phases 3 to 5 that guard what users get back
  (`release-rows`), Phase 6's latency rows inside 7c's gate (T4, T5), the freeze-readiness review of
  every format the release writes (D30, `format-freeze`), and `?nearest`, `?fuzzy` and `INFO`'s
  terms back (`more-like-this`).
- Phase 8's gate: a clean build without Xapian, every suite green, a license audit of every
  dependency, and an MIT source and binary release (strata `docs/PLAN.md` Phase 8;
  `docs/status/license-audit.md`, three blockers).
- The closing benchmark, run once and published, losses as losses: Xapiand on strata against the
  final Xapian-only release, Elasticsearch and Quickwit end to end, and strata against Xapian 2.1.0
  (strata `docs/PLAN.md` Phase 8, owner 2026-10-18; owner rulings 2026-11-03).
- The rename (the owner picks the name after a trademark search), changelog, tag, Homebrew formula
  and docs site; the old name redirects and the HTTP API keeps its routes (road-to-release 8c;
  strata `docs/PLAN.md` Phase 8).
- The strata lane beside it: the cold tier's step 2 finished, then the hybrid-search go/no-go run
  (owner, 2026-10-28): C1 rerun with strata at 1,000-vector tenants plus optimization experiments,
  deciding hybrid steps 1 to 4 or dropping "best at AI retrieval" (road-to-release, "The strata
  lane"; strata `docs/status/cold-tier.md` on `design/cold-tier`).
- The engine's goals, every phase's gate measuring them (strata `docs/PLAN.md`, "Goals"): beat
  Lucene, Tantivy, Xapian, Elasticsearch and Quickwit on query speed, indexing speed, memory and disk
  by the Ratchets' margins (by Phase 6: queries at most 0.7x, indexing at least 1.3x, memory at most
  0.7x, disk at most 0.8x), correct at every gate; every applicable technique evaluated and kept
  only if it wins (Track P); distributed-first at hundreds of thousands of nodes (Tracks C and D,
  Phases 3R and 7b); test parity with the Lucene, Tantivy and Xapian suites; memory tracked and
  minimized with no garbage collector.
- Pace, as the lead estimated it on 2026-11-01: about four to five weeks to the release, the weekly
  quota the binding limit (road-to-release, "Estimate"). An estimate, not a commitment.

## Non-goals

- Moving `stored` blobs into strata within 7c: Q4 is undecided and out of 7c; blobs stay in
  `DataStorage` with I/O in chores (owner, 2026-10-29); Track D's D7 decides the blob store.
- A durable-acknowledgement option for Xapiand: a separate feature item (owner, 2026-11-03, closing
  benchmark Q2).
- Work the owner placed after 7c that follows the release (D39, D40): wildcards, fuzzy and partial
  words inside phrases (Phase 5, `phrase-expansion`), `_cjk_words` (`cjk-words`) and `min_score`
  (owner, 2026-10-29). "More like this" comes before it (`more-like-this`, D40).

Not non-goals, but after the release (blocked quests with the owner's reason): 7b, the cluster
(shards as groups, the journal replacing the WAL, the shard RPC) (owner, 2026-10-29, D9); Tracks C and
D and Phase 3R, parked behind engine speed (owner, 2026-10-20, D4): strata's peer transport,
simulator and durability modes; Xapiand's failover and PKI fold into 7b and Track C. The recovery
tooling waits for the engine to be complete (owner, 2026-10-18, D28).

## Constraints

- No AWS or other cloud machines (owner, 2026-11-04, the host rule in the X-POOL ledger); campaign
  never rents machines. Linux runs go to the local QEMU VM through strata's
  `tools/vm/aws-linux.sh` with `STRATA_AWS_VM_ALLOW` unset, in a task-named directory; everything
  else runs on the macOS host.
- The machine is shared: builds and tests as `quiet light` jobs, measurements as `strict` or
  `exclusive` jobs of at most 30 minutes each. Timing rows are the lead's, on master, five
  interleaved trials with a bootstrap interval (`docs/design/phase7c.md` section 4); branches make
  functional claims only (strata `docs/PLAN.md`, "What waits", SYS-6).
- Identity: the Kronuz repositories are public. Author, commit and push as
  `Germán Méndez Bravo <german.mb@gmail.com>` with the `Kronuz` account; no tool attribution.
- Libraries arrive through CMake `FetchContent`; a bug in one is fixed in its own repository and
  Xapiand pins the fixed commit on that library's main, never a local patch (`AGENTS.md`). Results
  and how-it-works notes of library work go to that library's own `docs/`.
- Clean room: nobody writing the replacements opens `src/sortable_serialise.{cc,h}` or
  `contrib/python/xapiand-py/xapiand/utils/sortable.py` (`docs/design/sortable-encoding.md` on its
  branch; `docs/status/license-audit.md`).
- `docs/discussions/` stays byte-identical with its two other copies (`AGENTS.md`).
- Platforms: macOS arm64 and x86_64, Linux x86-64 and arm64, FreeBSD, as CI runs them.
- Every gate (strata `docs/PLAN.md`, "Every gate"): ASan and UBSan, TSan where threads are involved;
  one CPU-hour of fuzzing per decoder and parser added or changed, zero findings; readers reject
  unknown format versions with a fixture per format change; coverage at TESTING.md's targets and the
  phase's test-parity rows; a statement of each component's behavior at the target cluster scale.
- Formats stay open until the first release (owner, 2026-10-20, D30): pre-freeze versions, quick
  freezes as checkpoints; one freeze-readiness review before the first release that real data lives in.

## Reserved decisions

The owner decides these; everything else the lead decides and records below.

- Goals, non-goals and significant scope changes.
- Removing or narrowing user-visible behavior, including the seven agent-decided changes
  `docs/status/behavior-changes-audit.md` flags for the owner.
- Public APIs and formats the owner reserves: the value encoding (sortable-encoding Q1 to Q3), the
  strata format freeze for the release, the HTTP API routes, the product name.
- Commitments to other teams (Detent's lead), new external services or repositories, anything that
  costs money (the closing benchmark's metal window, Q7), and the hardware the bare-metal rows need.
- Deleting branches the triage archived.
- Carrying a ratchet row that misses its target into Track P or the next format (strata `docs/PLAN.md`,
  "Carried rows").

## Open questions for the owner

1. The product name, after a trademark search (Phase 8c).
2. Answered (D33): the owner had the 66 archived branches deleted on 2026-11-02; their `archive/*`
   tags stay (strata `docs/status/branch-triage-2026-10-30.md`).
3. Not the owner's: the default of the wide-search caps is the lead's call (`phase7c-wide-sr8.md`,
   "Decision for the lead"): `--max-indexes-per-search` (the pool size, 200) is gone; its successors
   `--concurrent-search-indexes` and `--max-search-indexes` (100,000 entries, about 2 per one-shard
   index since nested walks) need their defaults (wide-search and wide-sr8 ledgers).
4. The closing benchmark's host: its design publishes from an AWS `c7i.metal-24xl` (Q7: the metal
   window and `DAILY_USD`), which the 2026-11-04 host rule forbids; the non-AWS plan the owner asked
   for is `~/code/local-benchmarking.md` on the laptop (2026-11-02, untracked).
5. The value encoding: sortable-encoding Q1 (variable codec or the plan's 8-byte map), Q2 (the
   Python client's names), Q3 (NaN, out-of-range integers, -0.0).
6. Partly answered (D39, D40): of the work placed after 7c, "more like this" comes before the
   release (`more-like-this`) and the rest follows it. Still open: Q4 (what `stored` becomes) at all.
7. The "considerable" threshold of "one shape everywhere" (proposed: over 5% CPU or 10% p99 after
   optimizing).
8. Answered (D30): no format freezes before the release; one freeze-readiness review happens before
   the first release that real data lives in (format 13 left K1 to K5, K7, K10, K12 and K14 unticked
   and stays pre-freeze; sortable-encoding T8 adds a per-shard marker).
9. Answered (D35, D36): no bare-metal machine for now, and no arm64 machine beyond GitHub's CI; the
   rows that need one stay open until one exists. The M1 was retired on 2026-10-14, and the rows that
   need bare metal or arm64 have no host (Phase 0's hop and light-query p99, Phase 2's termdict p99, Phase 3's
   FreeBSD and arm64 rows before Phase 3R, Track C's T6, the document gate's arm64 rows, Phase 3R's
   NVMe rows, Track D's 30-node cluster). Campaign never rents one.
10. Answered (D39): the release waits on the correctness and safety rows of Phases 3 to 6 and the
    freeze-readiness review; the ratchet rows, the document profile gate and Phase 6's backends
    follow it. The question was whether the gates come before the release or after it: strata's plan orders them before Phase 7, and the owner's split
    (2026-10-29) sends 7a and 7c, then Phase 8, to the release without naming them.
11. Answered (D37): `automata` is taken up (`automata-evaluation`, then `automata`). Where `automata` lives: its code is on strata's `phase5/automata`, written before the search code
    moved to prospect (2026-10-20).
12. Answered (D40): those Elasticsearch, Quickwit or their peers serve that fit strata or prospect naturally. Which removed Xapian-era features come back, and when (D38): the behavior-changes audit's "Removed features" (Xapiand `docs/status/behavior-changes-audit.md`) lists them; Phase 5's relevance-feedback expansion and analyzers are where `?fuzzy`, `?nearest` and `_cjk_words` could return.

## Documents

What the product is goes to `docs/` of the repository it describes, never to `.campaign/`: Xapiand
`docs/design/`, `docs/gates/`, `docs/perf/`, `docs/mechanisms/`, `docs/dev/`; strata's, prospect's and
each library's own `docs/` (gate reports in each repository's `docs/gates/`). The earlier skill's
ledgers (`docs/status/*.md`) and reviews (`docs/reviews/*.md`) in Xapiand and strata stay where they
are as history: the history quests' journals cite them. strata's `docs/PLAN.md` holds the roadmap's
phase definitions, ratchets and gates; `docs/status/road-to-release.md` and strata's
`docs/status/README.md` are superseded by this file. Notes about running the campaign go to
`.campaign/xapiand/`.

## Milestones

In the roadmap's order (strata `docs/PLAN.md`); each phase holds its landed and its live quests.
History rows name the gate that closed them; future rows name their gate, which their gate quest runs.

| Milestone | Quests | Validation (a check that runs on `main`) |
|---|---|---|
| Phase 0 | `phase0` | History: Phase 0's gate closed at strata:a2d79a4a (2026-10-11): baselines in prospect `bench/phase0/`, the RESTORE attribution in strata `docs/FEASIBILITY.md` (strata `docs/reviews/phase-1.md`, "Phase 0 additions") |
| Phase 0.5 | `x1-analysis`, `engine-core`, `pool-concurrency`, `offload-bridge`, `prometheus-overflow`, `offload-followups`, `engine-query`, `engine-search`, `census`, `seam-census`, `cr1-fix`, `numeric-range`, `gate0-5` | History: gate 0.5 passed, `docs/gates/phase0.5.md` (run 1 Xapiand:79554ef8c, run 2 Xapiand:98e48e4eb, verdict Xapiand:5b865a059) |
| Phase 1 | `phase1` | History: Phase 1 gate APPROVED 2026-10-11, strata `docs/gates/phase-1.md` and `docs/reviews/phase-1.md` (strata:6a3cddde) |
| Phase 2 | `phase2` | History: Phase 2 gate APPROVED WITH CONDITIONS 2026-10-13, prospect `docs/gates/phase-2.md` and `docs/reviews/phase-2-gate.md` (strata:6932b812) |
| Phase 3 | `phase3-lifecycle`, `phase3-ratchets` | The Phase 3 gate: the lifecycle rows passed (strata `docs/gates/phase-3-lifecycle.md`, strata:2a111373); the ratchet rows on the gate VM, reported in prospect `docs/gates/phase-3-ratchets.md` by `phase3-ratchets` |
| Track C | `track-c-c1`, `track-c` | Per item: tests and measurements in `consensus` and `journal`, a gate report in strata `docs/gates/track-c-*.md`, the architect's review; C1 approved with conditions at strata:de99ec7e |
| Track P | `research-indexing`, `track-p` | Per item: a before/after table on all four axes in `docs/perf/`, kept or removed; `verify` and oracle parity green; the architect's review |
| Track D | `track-d` | Per item: the simulator at the scale contract (no acknowledged loss, no split placement, bounded convergence and fan-out), a real cluster of at least 30 nodes, the architect's review |
| Phase 3R | `phase3r` | The Phase 3R gate (strata `docs/PLAN.md`): 10^7 simulated steps over 1,000 seeds for 3 and 5 voters, linearizability, zero quorum-acknowledged loss, quorum-ack ingest against Elasticsearch, fsyncs per second at 1,000 groups |
| Phase 4 | `layers-refactor`, `format13`, `core-split`, `repo-split`, `core-lifecycle-tests`, `stored-short-section`, `phase4-gate` | The Phase 4 gate (strata `docs/PLAN.md`): Phase 3's ratchet rows; the filtered, sort, range, geo and aggregation classes; operator parity; aggregations equal across shards; reported in prospect `docs/gates/phase-4.md` by `phase4-gate` |
| The document profile gate | `core-vs-rocksdb`, `rocksdb-spec`, `format-14-kv`, `merge-wait`, `f14-step1`, `pressure-flush`, `flush-memory`, `retention`, `kv-scan`, `compact-slots`, `shared-reader-core`, `reclaim-backlog`, `merge-stream`, `flush-memory-bound`, `shared-reader-views`, `install-own-files`, `write-buffer-reads`, `reclaim-bg`, `write-buffer`, `flushmem`, `lanebuf`, `no-stalls`, `manifest-log-steps12`, `wbuf-inline`, `wbuf-inline-prospect`, `manifest-log`, `document-gate` | The key-value ratchet family against RocksDB (strata `docs/PLAN.md`): db_bench and YCSB A to F at 1x, 2x and 10x memory, every durability tier, four axes, Linux and FreeBSD; reported in strata `docs/gates/document-profile.md` by `document-gate`. Per landing, in `../strata`: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build`, and strata CI green on main |
| Phase 5 | `automata-evaluation`, `automata`, `phase5`, `cjk-words`, `phrase-expansion` | The Phase 5 gate (strata `docs/PLAN.md`): prefix, wildcard, fuzzy and expansion classes; stemmer vocabularies equal Xapian's; `queryparser` vectors; reported in prospect `docs/gates/phase-5.md` by `phase5` |
| Phase 6 | `phase6` | The Phase 6 gate (strata `docs/PLAN.md`): reactor p99 stall at most 1 ms; light-query p99 under load at most 2x unloaded plus 1 ms; at most 1 major reactor fault per 1,000 queries at 2x RAM; the Phase 6 ratchet rows; reported in `docs/gates/phase6.md` by `phase6` |
| Phase 7a | `fields-and-keys`, `strata-engine-write`, `strata-engine-search`, `field-table`, `seam-fields`, `strata-engine-callbacks`, `strata-engine-text`, `key-field`, `strata-fields`, `gate7a-fixes`, `phrase`, `t11-barriers`, `gate7a-l1`, `gate7a-l2`, `gate7a`, `xapiand-on-strata` | History: gate 7a passed 2026-10-29, `docs/gates/phase7a.md` (Xapiand:0d194335b), review strata `docs/reviews/phase-7a-gate.md` |
| Xapian-only release | `alpha10-builds`, `xapiand-final-release` | The final Xapian-only release from the `xapiand` branch (owner, 2026-10-28, D34): its backports, CI and the release workflows green on the branch before the tag; History: the branch builds on Linux, merged as Xapiand:dee2deaad |
| Phase 7b | `phase7b` | The distribution gate (strata `docs/PLAN.md` Phase 7): `harness/cluster_check.sh` and a power-loss replacement for `harness/wal_recovery_check.sh`; beats current Xapiand on every row and Elasticsearch and Quickwit on the claimed rows at 3, 30 and at least 300 nodes; Track D's scale gates; reported in `docs/gates/phase7b.md` by `phase7b` |
| Phase 7c | `phase7c-design`, `phase7c-s1`, `x1-delete-xapian`, `x1-tests`, `phase7c-l5`, `branch-triage`, `phase7c-x2`, `phase7c-x2b`, `phase7c-x3`, `x3-1-chunked-registry`, `strata-pins-2026-11`, `phase7c-x5`, `phase7c-x4`, `phase7c-x6`, `phase7c-x4-r1`, `pins-2026-11`, `phase7c-gate-prep`, `drop-threadpool`, `phase7c-linux-gate-fixes`, `x-pool-step1`, `phase7c-task-wave`, `linux-sanitizers`, `phase7c-wide-search`, `phase7c-http-wave`, `phase7c-wide-sr8`, `x-pool`, `perf-watch-peek`, `x7-gate` | `cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF && cmake --build build -j8 && ctest --test-dir build --output-on-failure && harness/async_check.sh --strict --owed --extern strata=.deps/strata-src && BIN="$PWD/build/bin/xapiand" harness/run_all.sh smoke e2e tls recovery`, and `docs/gates/phase7c.md` committed with T1 to T13 |
| Phase 8 | `license-audit`, `behavior-changes-audit`, `gate-prep-followups`, `direct-pins`, `name-index`, `sortable-proof`, `sortable-encoding`, `license-closure`, `docs-upgrade-page`, `postings-fuzz-memory`, `barrier-warning-completion`, `barrier-warning-strata`, `barrier-warning`, `closing-benchmark`, `release-rows`, `format-freeze`, `more-like-this`, `release` | The Phase 7c command, plus `! git grep -n -i -e sortable_serialise -e 'xapian\.h' -e '#include <xapian' -- src contrib` |
| the strata lane | `idle-index`, `cold-tier-step1`, `cold-tier-item5`, `cold-tier-step2-item1`, `cold-tier-step2-item2`, `cold-tier-step2-item3`, `cold-tier-step2-item4`, `cold-tier-step2-item6`, `cold-tier-step2-item5`, `cold-tier-step2-item5b`, `cold-tier-step2`, `hybrid-go-no-go` | In `../strata`: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build`, and strata CI green on main |
| Library family | `hashes-cl-1`, `hashes-fold32`, `sha256-cl-10`, `completion-cl-2`, `logger-producer-path`, `logger-producer-rings`, `stash-claim-hint-d64`, `stash-claim-hint-publish`, `library-issues` | Each library's own CI green on its main; the cpp-libraries gate's open issues (`docs/status/library-issues-2026-11.md`) closed or carried |
| Recovery tooling | `recovery-tooling` | Its gate (strata `docs/PLAN.md`): corruption of every file class at every offset in a fuzzed sweep; the tool never crashes, never writes into the damaged index, and recovers every document whose stored record is intact |

## Decisions

A `[decision]` criterion's evidence names a row's ID here.

| ID | Date | Decision | By | Why |
|---|---|---|---|---|
| D1 | 2026-10-18 | One closing benchmark before removal, every axis, two pairs: strata against Xapian 2.1.0, and Xapiand on Xapian against Xapiand on strata through the HTTP API | owner | strata `docs/PLAN.md` Phase 8 |
| D2 | 2026-10-19 | Every Xapiand change looks for generic pieces to extract; each removal gets an extraction-ledger row | owner | strata `docs/PLAN.md` Phase 7 |
| D3 | 2026-10-20 | 7a is single-node; opaque `bytes` slots first; `queryparser` moves onto the 7a path | owner | `docs/status/xapiand-on-strata.md` |
| D4 | 2026-10-20 | Distributed work parks behind engine speed | owner | strata status README; parked ledgers |
| D5 | 2026-10-26 | Once `queryparser` passes T4, Xapiand parses host-side for both engines | owner | `docs/design/xapiand-on-strata.md` |
| D6 | 2026-10-27 | A replaced layer must cost at most half of what it replaces (T11); parity fails | owner | `docs/design/xapiand-on-strata.md` T11 |
| D7 | 2026-10-28 | R-P2: no term lists in strata or prospect; R-RM: remove, don't refuse, features the strata engine does not serve | owner | `docs/design/xapiand-on-strata.md` section 7 |
| D8 | 2026-10-28 | When the cold tier lands, the hybrid-search go/no-go runs next | owner | strata `docs/status/cold-tier.md` (`design/cold-tier`) |
| D9 | 2026-10-29 | Phase 7's gate splits: single-node 7a and 7c, then Phase 8; 7b later; the single-node release has no cluster | owner | `docs/status/xapiand-on-strata.md` |
| D10 | 2026-10-29 | Non-Apple storage keeps the ordering barrier, with a warning, rather than a full flush by default | owner | `docs/status/xapiand-on-strata.md` |
| D11 | 2026-10-29 | Removed features stay removed unless someone would really use them; "more like this" and `min_score` proposed after 7c; queryparser phrase wildcards after 7c | owner | `docs/status/xapiand-on-strata.md` |
| D12 | 2026-10-29 | 7c Q1: delete the Xapian path at 7c's start; the closing benchmark compares against the final Xapian-only release | owner | `docs/design/phase7c.md` section 6 |
| D13 | 2026-10-29 | 7c Q2: Xapiand's WAL stays until 7b | lead (delegated) | `docs/design/phase7c.md` section 6 |
| D14 | 2026-10-29 | 7c Q3: http drops its offload path entirely; Xapiand, prism and Detent migrate | owner | `docs/design/phase7c.md` section 6 |
| D15 | 2026-10-29 | 7c Q5: the twice-RAM page-fault row (T6) gates at Phase 6; Q6: read-your-writes stays the default | owner (Q5), lead (Q6, delegated) | `docs/design/phase7c.md` section 6 |
| D16 | 2026-10-30 | Before release, not a priority: emit the non-Apple barrier warning (completion `PosixIO`) and wire `full_flush_ordering` through strata and a Xapiand switch | lead | `docs/status/xapiand-on-strata.md` |
| D17 | 2026-11-02 | Dates are strict ISO 8601 as datetime's head parses them; the pin wave's other result changes are accepted | owner | `docs/status/xapiand-on-strata.md` |
| D18 | 2026-11-02 | After the in-family pin wave, Xapiand declares only the libraries it includes directly, each at the head every library pins; no override is kept | owner | road-to-release, carried conditions |
| D19 | 2026-11-02 | Wide searches use bounded concurrency in batches of K, no cross-index snapshot, a runaway cap (100,000, 400 above it) | owner | road-to-release, carried conditions |
| D20 | 2026-11-02 | One shape everywhere: http's loop, handlers, routes and `BodyStream` become `task::task`-based; Detent's lead owns task's cancellation design | owner | road-to-release, carried conditions |
| D21 | 2026-11-02 | 7b input: journal and consensus get coroutine drivers with Xapiand's four requirements binding (Detent:a2d999a) | Detent lead, lead | road-to-release, carried conditions |
| D22 | 2026-11-03 | Nested indexes are the heart of Xapiand and are never refused; patterns are path globs | owner | `docs/status/phase7c-wide-sr8.md` |
| D23 | 2026-11-03 | Closing benchmark: X is the `xapiand` branch; compare at "acknowledged before fsync" with E's default beside it; E and Q only; Q4, Q5 as recommended; losses published, not blocking | owner | `docs/status/closing-benchmark.md` (`design/closing-benchmark`) |
| D24 | 2026-11-04 | No AWS: Linux sanitizer batches run on the local QEMU VM | owner | `docs/status/phase7c-x-pool.md` (`phase7c/x-pool-step3`) |
| D25 | 2026-11-04 | The manifest log follows RocksDB's append-only manifests, replaced when they roll over and cleaned up lazily | owner | strata `docs/design/manifest-log.md` (`design/manifest-log`) |
| D26 | 2026-11-04 | wbuf-inline ruling (c): the lane's chunks grow by a 32nd of the lane, 4 KiB to 1 MiB, after a 1 KiB first | lead | strata `docs/status/wbuf-inline.md` (`perf/wbuf-inline`) |
| D27 | 2026-11-08 | Detent is off http's removed offload path (Detent milestone M1, reported by Detent's lead): road-to-release's pending item is satisfied | Detent lead | migration brief |
| D28 | 2026-10-18 | Recovery tooling (inspect, verify, salvage, rebuild, restore) is built after the engine is complete | owner | strata `docs/PLAN.md`, "Recovery tooling" (strata:002737fc, strata:199770f9) |
| D29 | 2026-10-15 | Phase 3's query rows carry while Phase 4 proceeds; on 2026-10-16 the term dictionary and postings disk rows carry to format 13 (the owner's call delegated); the next phase's gate does not pass while a carried row misses its original target | owner | strata `docs/PLAN.md`, "Ratchets", "Carried rows" |
| D30 | 2026-10-20 | Formats stay open until the first release: pre-freeze versions, quick freezes as checkpoints (format 13, then 14), one freeze-readiness review before the first release that real data lives in | owner | strata `docs/PLAN.md` (strata:aadc91fa, strata:68e3d298) |
| D31 | 2026-10-17 | Build ahead of the gates: each phase, track or design is built on its own branch as soon as written; reviews run alongside; gate rows are measured on main commits only | owner | strata `docs/PLAN.md` header (strata:a61f8e7d), SYS-6 |
| D32 | 2026-10-17 | Engine priority: strata is a lifecycle core that must beat RocksDB (the document profile gate) and search plugins that must beat Lucene and Tantivy; indexing's design target is 4.5x the fastest baseline per index on one thread | owner | strata `docs/status/README.md`, "Priority"; strata `docs/PLAN.md`, "Indexing speed" |
| D33 | 2026-11-02 | Delete the archived branches: the lead deleted all 66 (53 strata, 11 prospect, 2 termdict), keeping their `archive/*` tags | owner | the owner's instruction (2026-11-02, 15:43Z) and the lead's deletion (15:46Z), in the lead's transcript; strata `docs/status/branch-triage-2026-10-30.md` |
| D34 | 2026-10-28 | A final Xapian-only release of Xapiand from the `xapiand` branch, with backports (queryparser's nested-NEAR overflow, numeric id 0, integer edge cases), CI and the release workflows green before the tag; then the README points to the successor and the repository is archived; the closing benchmark's X is this release (D12) | owner | the lead's memory `xapiand-final-and-elysian.md`; `docs/design/phase7c.md` section 6 |
| D35 | 2026-11-08 | No bare-metal machine for now: the rows that need bare metal (Phase 0's hop and light-query p99, Phase 2's termdict p99, Phase 3R's NVMe rows, Track D's 30-node cluster) stay open until there is one | owner | the owner, 2026-11-08 |
| D36 | 2026-11-08 | No arm64 machine beyond GitHub's CI for now: arm64 rows CI can run stay in CI; the rest stay open until there is a host | owner | the owner, 2026-11-08 |
| D37 | 2026-11-08 | `automata` (strata `phase5/automata`) is taken up: evaluated at least, and implemented or ported where it belongs after the split if it is worth it, useful and efficient | owner | the owner, 2026-11-08 |
| D38 | 2026-11-08 | Some removed Xapian-era features come back; the owner names which (D11 kept them removed unless someone would really use them) | owner | the owner, 2026-11-08 |
| D39 | 2026-11-08 | The release waits on the rows a later release cannot undo, for what it ships: Phase 3's `verify` and `parity` across merges with deletes, Phase 4's operator parity against the oracle and aggregations equal over one shard and many, Phase 5's stemmer vocabularies for the languages Xapiand serves (`release-rows`); Phase 6's stall and light-query rows, which 7c's gate carries as T4 and T5; and D30's freeze-readiness review with every decoder fuzzed (`format-freeze`). The speed, memory and disk ratchets, the document profile gate, the unbuilt items of Phases 4 and 5, Phase 6's backends and its twice-RAM fault row (D15) follow the release. Each gate still passes whole at its own commit. The lead may move a row before the release; moving one after it needs the owner | owner (delegated) | the owner, 2026-11-08: "whichever you deem are needed". Wrong results, lost writes and unreadable formats cannot be fixed by a later release; speed can, and the closing benchmark publishes the standing, losses as losses (D23) |
| D40 | 2026-11-08 | A removed feature returns when Elasticsearch, Quickwit or their peers serve it and it fits strata or prospect naturally. Of the behavior-changes audit's ten: #10 `?nearest` and `?fuzzy` return as Elasticsearch's `more_like_this` (also OpenSearch, Solr, Tantivy), with #8 `INFO`'s terms as its `_termvectors`, on one mechanism (a stored document's terms through the schema's analyzers, weighted by prospect's term statistics; nothing stored, so R-P2 holds), before the release, because master ignores the parameters silently (`more-like-this`). After the release: #11 `_cjk_words` on Phase 5's segmenter, as Elasticsearch's ICU, Kuromoji, Nori and SmartCN analyzers and Quickwit's `chinese_compatible` and `multilang` tokenizers do (`cjk-words`); wildcards, prefixes and fuzzy words inside phrases and NEAR on `automata`, as Elasticsearch's `intervals` and `match_phrase_prefix` and Tantivy's phrase prefix do (`phrase-expansion`), then #15's `_synonym` over them in Xapiand. #3, the cluster, returns with 7b (D9). Stay removed: #1 (Xapian itself); #5 and #6 (knobs and metrics of a pipeline that is gone); #7 `_elite_set` (no peer exposes it; `more_like_this` selects terms the same way inside); #9 `_terms` over `text` (Elasticsearch refuses it unless fielddata is on, Quickwit and Tantivy aggregate only fast fields' whole values, and it needs the term lists R-P2 forbids; a `keyword` field serves word facets); #12 (no peer's script sandbox loads code) | owner (rule), delegated | the owner, 2026-11-08: "From Elasticsearch, Quickwit, et al... Whichever naturally fit strata and prospect" |
| D41 | 2026-11-09 | Manifest log, the design's owner questions: Q1 the manifest log lands first, the standalone turn log (K2) after it as its own design and build; Q2 `retained_predecessors` stays as a count of generations, default 0, and `fallback()` reports the damaged generation; Q3 a lost write of the last edit reads as torn, as the turn log's last turn does | owner | strata `docs/design/manifest-log.md` "Questions for the owner" (`design/manifest-log`); quest `manifest-log` A10 |
| D42 | 2026-11-09 | Manifest log, SR3's deleted set in a decoded segment: adaptive, bitset words when they are no larger than the ordinals (deleted >= doc_count / 64), sorted ordinals otherwise; a bitset record decodes to words, a deltas record to ordinals (memory at most 8x the record); the index converts to `deleted_bits` at open after the size check | owner | quest `manifest-log` A4, journal Blocked on 1 (option 1) |
| D43 | 2026-11-09 | CJK word segmentation: our own segmenter in Kronuz/analysis (UAX #29 rules plus ICU's cjdict dictionary, Unicode-3.0 and BSD), built first as its own quest; no ICU at runtime, ICU only as the test oracle; `cjk-words` consumes it | owner | quest `cjk-segmenter`; `cjk-words` journal Blocked on (Xapiand:040a84c73) |
| D44 | 2026-11-09 | sortable-encoding, the owner approves building the critical format change (PLAN open question 5) and answers: Q1 (a) variable width, Kronuz/sortable's codec (exact, cross-type, 4.25 B per value); Q2 (a) the Python client exports `xapiand.utils.sortable.encode`/`decode`, old exports deleted; Q3 confirmed: NaN and out-of-range integers answer 400, -0.0 indexes as 0 | owner | `design/sortable-encoding` `docs/design/sortable-encoding.md` "Open questions"; quest `sortable-encoding` A1 |
| D45 | 2026-11-09 | The public docs site is refreshed as a whole, not only page by page: an audit now (docs-site-audit), a full rewrite once sortable-encoding lands (docs-site-refresh: architecture, features, quick-start, usage, configuration, reference), then one refresh quest at each milestone close and a final pass inside release. | owner |
| D46 | 2026-11-09 | strata's `phase5/automata` is ported (automata-evaluation, prospect `docs/design/automata.md`): `automata.hh` to a new public MIT header-only `Kronuz/automata` library depending only on `unicode` (Q1); the expansion rewritten in prospect `expand.hh`; Xapiand keeps only the query mapping (its `pattern.cc` scan goes); fst's automaton concept deleted; the branch archived after the port. Q2: matched terms with no live document are skipped (one postings probe per matched term on segments with deletes). Q3: edit distance is served on the key field. Q4: string-similarity's code-point OSA distance (O5) is dropped. | owner |
| D47 | 2026-11-09 | The docs site is the E2E suite: every `rest` request on a page runs, and `<!-- e2e:begin --> ```js pm.test(...)``` e2e:end -->` blocks assert its response (`docs_to_postman.py` extracts them for newman, `harness/run_all.sh e2e`). Any page edit keeps or fixes its tests in the same change; every feature Xapiand serves gets a page with requests and pm.test assertions until the docs and E2E cover all or most features; the coverage inventory (feature, page, tests) lives in `docs/dev/docs-coverage.md`. Docs pages count as code for evidence (`campaign config tested_docs`). | owner |
| D48 | 2026-11-09 | GitHub triage (github-triage): close as not planned #24, #26, #34, the at-rest half of #25, and #28 if no retest after the reply; post #33's license reply as drafted, naming releases up to alpha.10 as Xapian-based; #27's reply omits any Elasticsearch comparison until the published benchmark; #21 (result highlighting) and #14 (n-gram index and search) become quests after the document gate. | owner |
| D49 | 2026-11-09 | Fable trial (owner delegated the call): the next two high-risk reviews run twice from fresh contexts on the same commit, Opus at high and Fable at max: manifest-log durability and write-path-fixes correctness (the version race). Compare blocking findings unique to each, false positives and tokens. Fable materially ahead on either → a frontier tier in the campaign skill for critical reviews and design gates (sortable-encoding's first), and the result goes to Detent's lead; otherwise Opus stays. Both reviews count; their findings merge. Owner, 11-09: Fable and frontier models are used sparsely, on targeted hard or sensitive tasks only; a won trial adds a tier for those, never a default. Trial 1 (manifest-log durability at strata:e0508e9, 11-10): Opus (high, ~200k) found 2 blocking defects Fable missed (unsynced rollover rename, failed sync_data bytes), both confirmed (job 261110-022246-d64be6); Fable (max, ~227k) found 1 blocker Opus durability missed (ENOSPC soft bound), also found by the Opus security reviewer, with a sharper probe (261110-023641-b98c95), and 1 unique non-blocking (fence message before fenced_); 0 false positives each. Trial 1: Fable not ahead; trial 2 (write-path-fixes correctness) decides. | lead, owner Trial 2 (write-path-fixes correctness, 11-10): Opus high (187k) and Fable max (~230k) found the same two defects with opposite severity: Opus blocked on the multi-shard DELETE docid race with a reproducing probe (Fable: non-blocking F2), Fable blocked on docid numbering wrapping past docid::max with a probe (Opus: non-blocking N3). Lead made both blocking. No unique finding either way; Fable again not ahead at about 1.2x the tokens. |
| D50 | 2026-11-10 | Range aggregation default keys (query-dsl-fixes review F2): a bucket's default key is formatted from the request's bounds as written, not the effective (rounded or clamped) bound, so `{_from:2.5}` and `{_from:3}` on an integer field stay two buckets keyed `2.5..` and `3..`, each counted once. Buckets keyed by range index stay out of scope. | owner |
| D51 | 2026-11-10 | phase3-ratchets rulings: C15 takes allocation counts at current code; C7 carries to the next phase; C9 and C15's rest go to a follow-up quest; A4's indexing budget miss (peak RSS 1.57x the node budget, limit 1.1x; merge buffers not charged) stays reported as a miss and goes to a fix quest that charges merge buffers to the budget; A3's many-index rows get their own harness quest, high priority (many-indexes is the core workload); A8 runs FreeBSD as a deferred check, arm64 waits for a local host (no AWS). | lead |
| D52 | 2026-11-10 | docs-site-refresh no longer lands after sortable-encoding: the order was a cycle (sortable-encoding needs changelog.md and README.md, which docs-site-refresh holds). sortable-encoding updates the value-encoding pages and the About page's licence sentence itself when it switches; docs-site-refresh adds docs/src/components/** (the splash #_top fix) | lead | quests/docs-site-refresh.md, quests/sortable-encoding.md |
| D53 | 2026-11-10 | x7-gate rulings. A23: T4 reads the control loop's lag only while it is busy; an idle 1 ms probe would wake every loop 1,000 times a second, which the many-indexes workload pays for and no request feels. A21: "no worse than head" means head's value lies inside the 95% bootstrap interval of the new run or above it (five interleaved trials), the same rule as the timing rows. Gaps the inventory found become quests x7-http-repin (A24), x7-cancel-rows (A9), x7-view-bench (A22) and x7-coverage (A13); A6 and A15 need a local Linux VM, set up by the lead (no AWS) | lead | quests/x7-gate.md and the four quest files |
| D54 | 2026-11-10 | The 7b durability design (strata docs/design/durability-modes.md, parked) must answer two owner concerns before the WAL goes: (1) a remote leader puts a network hop before every 2xx, where today's local WAL answers at once: routing writes to the leader (a published shard-to-leader map, one forward at most), leader placement near writers, and whether a node-local accepted level for unconditional writes is offered; (2) what replaces `?commit`, which today runs a full strata commit (segments and manifest synced, visible, covering every pending write of the shard): `ack=quorum` plus `refresh=true` gives the same survive-a-crash and visible promise through journal replay, and a checkpoint level (Elasticsearch's flush) is kept for callers who need the write in synced segments; `all` adds every replica and is not the equivalent | owner | quests/phase7b.md |
| D55 | 2026-11-10 | docs-site-refresh splits: it lands its getting-started and architecture areas as the whole quest (an increment cannot be the whole branch), and the reference guide, tutorials, the whole-site link check and the coverage map move to docs-site-refresh-reference, after it | lead | quests/docs-site-refresh.md, quests/docs-site-refresh-reference.md |
| D56 | 2026-11-10 | x7-gate's first rows. T1's census finds `Xapiand:DB-TIMER` (the catalog's timer, x-pool): it moves onto the debouncer wheel (quest x7-catalog-timer), not into section 2's list. T8's admission and demotion peaks are not exported: x7-cancel-rows A4. T8 at 16 reactors needs about 20,000 client connections, past one address's 16,384 ports: T8 gates the per-reactor caps, so it runs at 8 reactors on macOS with the count stated, and at 16 on the Linux VM with several 127.0.0.x source addresses | lead | quests/x7-gate.md, x7-catalog-timer.md, x7-cancel-rows.md |
