> Status: CURRENT (2026-11-03). Audit of every user-facing behaviour change in Xapiand since
> v1.0.0-alpha.10 (`Xapiand:3fe13a2fb`): local master `Xapiand:b4636078d`, plus branches
> `phase7c/http-wave` and `fix/wide-search-sr8`. Sources: the changelog's `[Unreleased]`, `git log
> v1.0.0-alpha.10..master` (338 non-merge commits), an option-by-option diff of `src/opts.cc` and
> `src/reserved/`, the owner rulings (this directory's `xapiand-on-strata.md` and `road-to-release.md`,
> `../design/xapiand-on-strata.md` R-P2 and R-RM, the memory notes), and Xapiand's `docs/design`,
> `docs/reviews` and `docs/status`. Read-only; nothing compiled or run.

# Xapiand behaviour changes since alpha.10: who decided

## Summary

49 changes. Pure internal changes, additive features and bug fixes that only turn a crash into an
error are left out (listed at the end).

| Category | Count | | Decider | Count |
|---|---|---|---|---|
| Removed feature | 10 | | Owner ruling | 16 |
| Narrowed input (now refused) | 11 | | Reviewer or architect | 14 |
| Changed output or semantics | 14 | | Agent's own choice | 19 |
| New limit | 10 | | | |
| New default | 4 | | | |

"Agent" includes the lead: a lead ruling is an agent's choice unless an owner ruling covers it. Where a
reviewer offered several fixes and an agent picked the one that refuses, the decider is the agent (the
nested-index refusal happened this way).

### Agent-decided, medium or high risk: the owner should review these

1. **#21 Nested index paths refused** (high). The security reviewer offered "descend into index
   directories" or "refuse nesting"; the developer picked refusal (`Xapiand:b6b454d9a`). The owner
   reversed it on 2026-11-03; the refusal is still on master and on `fix/wide-search-sr8` until the
   revert lands, and `Xapiand:ade61e145` renamed two docs examples to avoid nesting (they need restoring).
2. **#4 Removed options make the node refuse to start** (medium). `xapiand --solo`, `--cluster`,
   `--committers` and 30 others are unknown arguments, so an old systemd unit or Docker command line exits
   with a usage error. The rulings remove the features; none chose fatal over accepted-and-ignored
   (`harness/removed_options_check.sh` enforces it).
3. **#11 `_cjk_words` removed** (medium). Deleted with Xapian because "only Xapian served" it (X1 K3,
   `Xapiand:5a517b052`); it is host-side text analysis, not an engine feature, so R-RM does not name it.
4. **#14 Text query strings: a lone `-a` answers 400** (medium). The owner approved queryparser
   (2026-10-20, Q3); the per-row differences from Xapian's parser were decided by agents in queryparser's
   `test/vectors/parse-ab.jsonl`. The changelog files this under alpha.10, which did not ship it.
5. **#25 `_terms`/`_values` counts can be approximate; geo refused** (medium). A `shard_size` cap of
   `_limit * 1.5 + 10` buckets per shard (`Xapiand:db92c170a`). The code review only suggested the
   architect "may want" a cap (`docs/reviews/strata-engine-callbacks-code.md:61`).
6. **#29b A Lua error fails the write with 400** (medium). It used to store the document untransformed.
   A developer's choice (`Xapiand:a62c1277a`, N3); the security reviewer recorded it as a semantic change
   (`docs/reviews/phase7c-x4-security.md`, X4-R4).
7. **#43 Multi-index relevance weighs each index on its own statistics** (medium). Scores and order of a
   search over several indexes change. The lead's fix-round finding (`docs/status/phase7c-wide-search.md:49`);
   the owner's wide-search ruling covers batching, not the weighting.

Reviewer- or architect-decided medium-risk changes, also worth a look: #23 query size caps (4096
clauses refuses a large `_in` list), #26 the 16 MiB body cap (413 for any non-`stored` body and for a
chunked `RESTORE`), #28 Lua bounds, #29a per-call Lua globals, #30 a per-index cap on concurrent
searches (503 instead of waiting).

### Changelog gaps found

1. The queryparser change and the `_near`/`_phrase` client-error fix sit in the `[1.0.0-alpha.10]`
   section, but `Xapiand:f29dfe942` is not an ancestor of the tag; both are unreleased.
2. The 16 MiB buffered-body cap is stated only for `stored` uploads; every other body over it, and a
   `RESTORE` without `Content-Length`, now answers 413.
3. Cluster options removed but not listed: `--replicas`, `--replicators`, `--remote-clients`,
   `--remote-servers`, `--replication-clients`, `--replication-servers`, `--discoverers`,
   `--database-stall-time`.
4. "A data directory with Xapian indexes is refused" names alpha.9 or earlier; alpha.10 data is Xapian too.
5. Removed options are fatal at startup; the changelog says only that they are gone.

## Full table

Risk is to existing users: high (a common feature or workflow), medium, low (an edge case, a new feature
with no old behaviour, or a security fix with no legitimate use).

### Removed features

| # | Before → after (example) | Decider | Risk |
|---|---|---|---|
| 1 | The Xapian engine: alpha.10 indexes are unreadable; dump with the Xapian-only release and `RESTORE`. `XAPIAND_ENGINE` gone | Owner, 7c Q1 (`xapiand-on-strata.md`, "Owner answers on the 7c design") | High (every user reindexes) |
| 3 | The cluster: one node only; remote and replication protocols, Raft, discovery, `cluster_name`/`nodes` in `GET /`, Kubernetes guide | Owner, "Clustering (owner, 2026-10-29)" | Medium |
| 5 | `--committers`, `--fsynchers`, `--http-servers`, `--http-clients`, `--bulk-preparers`, `--bulk-indexers`, `--max-database-readers` | Owner Q3 (offload path) for `--http-clients`; the rest design D15, architect `docs/reviews/phase7c-design.md` | Low (tuning only; see #4) |
| 6 | `num_committers` in `GET /`; `xapiand_http_clients_*`, `xapiand_committers_capacity`, `_pool_size`; `versions.Xapian`/`ICU`; `#xapian_query` renamed `#engine_query` | Agent (`Xapiand:432f9a45f` X6 K1, `Xapiand:5a517b052` X1) | Low (dashboards) |
| 7 | `_elite_set` → 400 "Invalid operator" | Owner R-RM (`../design/xapiand-on-strata.md:371`) | Low |
| 8 | `INFO` on a document no longer lists its terms | Owner R-RM (same, names the `:info` term view) | Low |
| 9 | `_terms` over a `text` field → 400 | Owner R-RM | Medium (word facets) |
| 10 | `?nearest`, `?fuzzy` (+ `n_rset`, `n_eset`, `n_term`, `field`, `type`): now silently an ordinary search | Owner R-RM; lead's ruling `docs/status/gate7a-l1.md`; owner heard it 2026-10-29 with "more like this" proposed after 7c. Ignoring the parameters rather than 400 is the agent's | Medium (owner: "real users") |
| 11 | `_cjk_words` schema property → unknown; ICU dropped; queries no longer split CJK words | **Agent** (X1 K3, `Xapiand:5a517b052`) | Medium (CJK users; `_cjk_ngram` remains) |
| 12 | Lua: `load`, `dofile`, `loadfile`, `collectgarbage`, `string.pack/unpack/packsize/dump`, `<close>`, `__gc`/`__close`/`__mode` | Design D10, architect `phase7c-design.md`; security `phase7c-x4-security.md` | Low |

### Narrowed inputs (now refused)

| # | Before → after (example) | Decider | Risk |
|---|---|---|---|
| 2 | A data dir holding `iamglass`/`*.glass`: started with empty indexes → exits 65 | Reviewer X1-N6 (`docs/reviews/x1-delete-xapian-code.md:69`) | Low |
| 4 | `xapiand --solo` (or any removed option) started → usage error, exit | **Agent**, applying R-RM (`harness/removed_options_check.sh`) | Medium |
| 13 | `q=a NOT` / `a ()` / `(a` searched → 400; `a AND b c` regroups as `(a AND b) OR c` | Owner, 2026-11-02 (`xapiand-on-strata.md`, pin wave) | Medium |
| 14 | `{"title": "-a"}` searched the word `a` → 400; `+a b -c d` window differs | **Agent** (queryparser A/B rows); owner approved only the library (Q3, 2026-10-20) | Medium |
| 15 | `_synonym` over `colou*r`, a fuzzy word or a group; an array to `_scale_weight`/`_wildcard`/`_edit_distance` → 400 (Xapian searched them) | **Agent**: lead's ruling `gate7a-l1.md` after architect L1 (`../reviews/phase-7a-gate.md`) | Low |
| 17 | Bare-LF request answered 200 → connection closed; also obs-fold, `Transfer-Encoding: chunked, identity`, bad `Content-Length` | Owner for bare LF (2026-11-02); the rest the http-parser library's (agent) | Low |
| 18 | `-D ""` meant cwd → usage error; `PUT /a/../doc`, `/./doc` → 400 | Owner, 2026-11-02 | Low |
| 19 | `PUT /%2Ftmp%2Fx/doc`, a `_foreign` link to `/tmp/x` wrote outside the data dir → 400 | Owner, 2026-11-02 (paths outside the base) | Low |
| 20 | Index paths with NUL, control bytes, a backslash or malformed UTF-8 → 400 | Reviewer SR1 (`docs/reviews/phase7c-gate-prep-security.md`) | Low (a `\` in a name was legal) |
| 21 | `PUT /twitter/user/d` beside index `twitter` → 400 (also above an index, or a `strata`/`.__N` component) | **Agent** picked refusal from reviewer SR2's options (`phase7c-wide-search-security.md:74`, `Xapiand:b6b454d9a`). **Owner reversed 2026-11-03**; revert pending on master and `fix/wide-search-sr8` | High |
| 22 | YAML bodies with `&anchor`/`*alias` → 400 | **Agent** picked refusal from reviewer SR3b/c's options (`strata-engine-text-security.md:478`, `Xapiand:7d3568161`) | Low |

### Changed output or semantics

| # | Before → after (example) | Decider | Risk |
|---|---|---|---|
| 16 | `2020-01-01T10:00:00.Z`, >62-digit fractions: typed datetime (and 400) → typed text; `2020-12-15\|\|+12M` works | Owner, 2026-11-02 | Low |
| 25 | `_terms`/`_values` counts exact → may fall short past `_shard_size` (default `_limit*1.5+10`); over geo → 400 | **Agent** (`Xapiand:db92c170a`) | Medium (silent undercount) |
| 29a | Lua globals persisted across calls of a script → one call only; `table.sort` stable; `print` 4 KiB | Design D10, architect `phase7c-design.md` | Medium (scripts keeping state) |
| 29b | A script that raises stored the document untransformed → 400 | **Agent** (`Xapiand:a62c1277a` N3); noted by X4-R4 | Medium |
| 38 | `_sort` by `soundex` against `DICKSONX`: `DWAYNE, DUANE, DIXON` → `DICKSONX, DIXON, DWAYNE`; Jaro, Dice, weighted Levenshtein corrected | Owner, 2026-11-02 | Low |
| 39 | On Apple silicon/`-mfma`, 0.7% of points near a trixel edge get another id | Agent (htm library fix) | Low (reindex is forced anyway, #1) |
| 40 | `{"a":1,"a":2,"b":3,"c":4}` stored `{"a":2,"b":3,"b":3}` → `{"a":2,"b":3,"c":4}` | Agent (msgpack library fix) | Low |
| 41 | `a/b/..` is path `a`; `../a` is path `..` with id `a`; empty base is `.` | Owner, 2026-11-02 (`normalize_path` keeps `os.path.normpath`) | Low |
| 42 | Node names and seeded random draws differ | Agent (namegen, xoshiro256++) | Low |
| 43 | `SEARCH /a,b/`: one statistics pool → each index weighed alone (scores and order move) | **Agent**: lead's fix round (`docs/status/phase7c-wide-search.md:49`) | Medium |
| 44 | A search held its index (blocked writes) → reads a view as of its start | Design D5/D6, architect | Low |
| 45 | A write whose commit wait timed out → error; now 2xx, commit left to the timer | Agent (`Xapiand:1afabbd06`, X2) | Low |
| 46 | `?collapse=age&limit=5` with `collapse_max` > 1: Xapian's lower-ranked docs → the best per group | Agent: lead's ruling "serve collapse" (`gate7a-l1.md`) | Low |
| 47 | `_terms` reads stored values: keyword buckets keep case (`Red`, `red`); numeric fields return values, not accuracy terms | Owner R-P2 (`../design/xapiand-on-strata.md:363`); the case split is a consequence the ruling does not name | Medium (faceting) |

### New limits

| # | Before → after (example) | Decider | Risk |
|---|---|---|---|
| 23 | Query: 100 operator levels, 4,096 terms+operators, 256 phrases per AND, DSL 45 levels, body 200 levels → 400 | Reviewer SR1/SR2/SR1b (`docs/reviews/strata-engine-text-security.md`) asked for bounds; values agent (`Xapiand:3612b7a57`, `c1fca3f6c`) | Medium (an `_in` or OR of >4,096 ids) |
| 24 | 65,535 live buckets per search, 16 nesting levels → 400 | Agent (`Xapiand:e3087bcac`) | Low (Elasticsearch's default) |
| 26 | Body cap 16 MiB: any non-`stored` body over it, and a `RESTORE` sent chunked, → 413 (alpha.10 had none) | Design D12 "an estimate", architect `phase7c-design.md` F7 | Medium |
| 27 | `stored` blob >16 MiB streams: needs `Content-Length`; `POST` without id → 400; >4 GiB → 413 | Agent: designer's reversible choice 2 (`docs/design/phase7c.md` section 6) | Low |
| 28 | Lua per call: 10^8 instructions, 5×10^8 work units, 64 MiB, 16×10^6 units per library call → 400; result >16 MiB or >262,144 value units → 413 (whole doc counts); stack 200,000 | Design D10 "estimates", architect; security X4 | Medium (scripts over big documents) |
| 30 | Concurrent searches of one index past 16 per loop + workers → 503 (waited before) | Architect ruling X2b (R3 in `Xapiand:70c40cc78`; `docs/reviews/phase7c-x2b-code.md`) | Medium (bursty readers) |
| 31 | A loop with 256 demoted requests → 503 `Retry-After: 1` | Design D2 "an estimate", architect | Low |
| 33 | `--max-search-indexes` 100,000 → 400 | Owner, 2026-11-02 (`road-to-release.md`, "Wide searches") | Low |
| 34 | Patterns: 32 per search; the walk's entries read count against 100,000 (a node with more entries cannot use a wildcard); 4,096 match steps per entry; one walk per node at a time | Reviewer SR1 (`phase7c-wide-search-security.md`); values agent. New feature, but conflicts with the many-indexes workload (`road-to-release.md` records it as design debt) | Low |
| 35 | (`fix/wide-search-sr8`) 8 waiting walks → 429; 10 s wait → 503 (`--search-walk-queue`, `--search-walk-wait`) | Reviewer SR8; values agent (`Xapiand:5c895d94c`) | Low |

### New defaults

| # | Before → after (example) | Decider | Risk |
|---|---|---|---|
| 36 | Shutdown waited without limit → 35 s bound, then exit 75 abandoning in-flight work | Agent (X5/X6, design D16) | Low |
| 37 | (`phase7c/http-wave`) Stop: stalled clients' sockets close after a grace (about 15 s) | Agent (`Xapiand:1e9b90042`) | Low |
| 48 | HTTP loops capped at 10 → one per processor, uncapped; fsync pool from `--processors` | Design D2 C5, architect | Low |
| 49 | `--concurrent-search-indexes` defaults to a loop's share of the pool | Owner, 2026-11-02 (K from the reader cap) | Low |

## Left out

Additive: `_sort` order as a string, `xapiand_reactor_lag_seconds`, streamed blob uploads, `_all` and
wildcard index lists, 64-bit hit ids. Fixes that only turn
crashes or 500s into 400s: deep MessagePack/YAML, `_near` over a boolean, 150-level field paths, NUL
`..%00`, msgpack counts, root routes, quarantine on macOS, `RESTORE` version bump, setup failure exit 70.
Internal: pins, `WorkPool`, the control loop, single-flight schema loads, segment version bump (strata
was not in alpha.10). The turn wait (30 s, then the same 503-class error) is not new.
