> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-02) the gate-prep developer's ledger (phase 7c, before X7): the owed async rows, D16, absolute index paths, the security-review fixes (SR1 and the rest), dead includes; local branch `phase7c/gate-prep`, not pushed.

# phase7c gate-prep (build): get master ready for the X7 gate

Branch `phase7c/gate-prep` in `Xapiand-wt/gate-prep`, from master `Xapiand:81c311799`. Design: `docs/design/phase7c.md` (D1, D4 to D7, D14, D16, the T rows); the carried conditions: strata `docs/status/road-to-release.md` ("Before X7").

## State

All four criteria are built and proved on Release and ASan+UBSan. The security review (`docs/reviews/phase7c-gate-prep-security.md`, APPROVE WITH CONDITIONS) is folded in: SR1 and its root cause, `GET /`, the clean 400s and the newman preflight (section "Security-review fixes" below). One step is not mine: the SR1 root cause is in the `fs` library, so its fix is on a local `fs` branch that the lead pushes and pins (Next). Sensitive areas touched: an untrusted-input path check (`Endpoint`), a new byte validator, and the `fs` quarantine buffer; the SR1 re-check is owed to the security reviewer.

## The owed rows

`harness/async_check.sh --strict --owed --extern strata=<strata at the pin>` listed 27 hits in 10 rows (X6's ledger said 31; its four Lua rows left with X4). Every row is now `allow` or its code is deleted; the list has no owed row, and `--strict --owed` exits 0.

| Row (file, hits) | Decision | Reason |
|---|---|---|
| `database/shard.cc` `storage_commit` (2) | allow (D7) | the call is `co_await storage_commit(chores_)` and the body is `chores_.run(flush, ...)`: the blob commit already runs in the commit chore |
| `engine/strata/shard.cc` file (4: `fs::exists`, `create_directories`, the path joins) | allow (D4) | `acquire` runs only in a query chore (`open_local`, `check`), never on a reactor |
| `engine/strata/shard.cc` `entry->` mutex (2) | allow (D4, D5) | `acquire` holds the entry lock across the open I/O inside a chore, and only while the core is absent, i.e. while no `Handle` exists, so no `~Handle` can wait on it; `~Handle` takes it for an O(1) count. A new open of a path waits for the old entry's `closed` event, which is set after its engine is released |
| strata `index.hh` `drain()` (4: the definition, `apply`, `install`, the merge reservation) | allow (D6) | all four are storage admission. Xapiand sets no storage permit, so `reserve` is 0 and a merge reserves nothing; `apply` under `defer_io` (which `index_options` sets for a host) answers `drain_owed` and never drains; `install` is not called |
| `database/utils.cc` `read_uuid` (4) | deleted (X1) | read the Xapian `iamglass` file; a data directory with Xapian files is refused at startup, so it only ever returned -1. `DatabaseWAL(path)` starts with `validate_uuid` off, as it did |
| `database/wal.cc` `init_database` (11) | deleted (X1) | wrote `iamglass` and `postlist.glass`; nothing called it |

`registry_mutex`'s note no longer says "moves to D5's registry" (it stays: O(log n), no I/O).

`wal_view` (the `WAL` method) used to read the whole write-ahead log in `DatabaseHandler::repr_wal` on the request's reactor, and no rule matched the `Storage` reads. Fixed: `repr_wal` is a task that reads the log in a query chore of its own `IndexChores` group; `wal_view` awaits it. New `wal` rule in `async_check.py` (a `DatabaseWAL` construction or a `.repr_wal(` call) with three allow rows: the chore body in `handler.cc` (anchor `DatabaseWAL wal\(path\)`), the open chore in `shard.cc`, and the awaited call in `search_views.cc` (anchor `co_await db_handler\.repr_wal`).

## Absolute index paths (criterion 3)

**Reachable over HTTP, and from the CLI. Fixed: 400 / usage error.** An absolute path reached `Endpoint` unchecked (only relative paths were bounded) and became the filesystem path of the index, because `fs::path(path)/"strata"` ignores the process directory for an absolute path.

Entry points found (every `Endpoint(` construction in `src/`):

| Entry | Absolute path reaches Endpoint? |
|---|---|
| HTTP index routes (`expand_paths` -> `Endpoint(path)`) | yes, when the decoded path starts with a second `/`: `/%2Ftmp%2Fx/doc`, `/%2Ftmp/x/doc`, with `%2F..%2F` or `%2e%2e` inside. A literal `//tmp/x/doc`, `///tmp/x`, `/.//tmp/x` and `/a/%2Ftmp%2Fx` are folded by the router into the data directory (inside, harmless) |
| `_schema` with `_type: foreign/object` and `_foreign: "/tmp/x/doc"` (`schemas_lru.cc` `Endpoint(foreign_path)`) | yes; the first document write creates the index there |
| Lua / `_foreign` script link (`read_foreign_script`) | yes, same `Endpoint(foreign_path)` (read side) |
| query DSL | no Endpoint is built from a query |
| CLI `--dump`, `--restore` (`cli_endpoint`) | yes: `--restore /tmp/x` wrote an index there (exit 0), `--dump` exited 70 |
| CLI `-D` | not an index path: the data directory itself, `-D ""` already refused |

Probe: `harness/path_boundary_check.sh` (extended), run against the binary without the fix (`Endpoint` unchanged) and with it, same script:
- Before: `PUT/GET/DELETE /%2F<abs>/doc` gave 204/200/204 and an index with its shards `.__1` to `.__5` appeared in `<abs>`; a foreign schema in `<abs>/sch` took its first write with 204 and also created shards there; `/%2Fetc%2Ffoo/doc` gave 500; `--restore <abs>` exited 0 and `--dump` 70. The first run said "nothing outside" because `ls` hides `.__1`; the check uses `ls -A`.
- After: all of those answer 400 (CLI 64); the folded forms stay inside the data directory; nothing outside changes.

The fix: `Endpoint` refuses any path, relative or absolute, that is empty or not strictly inside `Endpoint::cwd` (`src/endpoint.cc`). An absolute path inside the data directory is still accepted and stripped to its relative form. A symlink inside the data directory that points out is not covered (a filesystem property, not a path one).

## Proof

Builds: Release (`BUILD_TESTS`), and ASan+UBSan (Homebrew LLVM 22, RelWithDebInfo, `detect_leaks=0`), at `Xapiand:9fcdf2dd1`.

| | Release | ASan+UBSan |
|---|---|---|
| ctest | 32 of 32 | 32 of 32 |
| docs E2E (`harness/engine_e2e.sh`, newman 6.2.3), 371 requests | 641 of 641 | 641 of 641 |
| `graceful_shutdown_check.sh`, `path_boundary_check.sh`, `cli_roundtrip_check.sh` | all rc 0 | all rc 0 |
| no "blocks on" and no sanitizer line in the E2E node log | yes | yes |

- `async_check.sh --strict --owed --extern strata=<the pinned strata>` (the CI form): 244 hits, allow 231, owed 0, compiled out 13, unclassified 0, exit 0. `python3 -I harness/test_gate_tools.py` 15 of 15; ruff clean.
- Fails without each change:
  - Owed rows: `test_the_tree_has_no_owed_hit` fails with the allowlist reverted (owed D4, D5, D7 rows come back) and with `wal.cc` reverted (11 unclassified `iamglass` hits).
  - Endpoint: `test_endpoint` on the binary without the fix: 16 of 76 checks fail; `path_boundary_check.sh` on it: the failures listed above (rc 1); with the fix, rc 0.
  - Dead includes: nothing to test (compiles; a use would not compile without them).
- Mutant (Release): `Endpoint` comparing the base without its trailing slash (so `/var/db/xapiand2/x` passes) -> `test_endpoint` fails 2 of 76 checks (killed). Source restored.
- New parsers or decoders: none (no fuzz target owed). The `Endpoint` change is a path check on untrusted input; its cases are table tests.

## The `wal_view` chore

- Proof: with the three source files reverted, `async_check.sh --strict --owed --extern strata=<pin 501f9437>` exits 1 (`handler.cc:254 [wal] unclassified: DatabaseWAL wal(endpoints[0].path);`, `search_views.cc:1757 [wal] unclassified: auto obj = db_handler.repr_wal(...)`), and `test_the_tree_has_no_owed_hit` fails; with the fix it exits 0 (247 hits, allow 234, owed 0, unclassified 0). Mutant: dropping the `wal` rule fails `test_every_rule_class_is_found` (and the tree test). `test_gate_tools.py` 15 of 15 with the fix.
- Release build (RelWithDebInfo-like `-O3`, ASSERTS) ctest 32 of 32. Live: `GET /walt/.__N/:wal` and `?raw` answer the log through the chore; an index of 5 shards still answers 400 (unchanged). Sanitizers: not run for this step (the change is a lambda over locals awaited in the frame).
- Docs E2E: no docs page documents `:wal`. newman 6 under Node 26.10 on this host exits silently on the third request (with `localhost`: `ERR_INVALID_IP_ADDRESS`), so the full suite could not run; a status-only replay of the collection gave 337 ok, 34 mismatches, all from missing `@file` fixtures (bank, Lenna) and unencoded spaces in my replayer, none on `:wal`.

## Security-review fixes

### SR1: a NUL in an index path (Critical)

`Endpoint`'s constructor now refuses any path that is not well-formed UTF-8 or holds a NUL, a control byte (below 0x20), DEL or a backslash, before the prefix check (`src/index_path.h`, `index_path::valid`; `src/endpoint.cc`). 400 over HTTP, usage error 64 on the CLI. The direct route, `_foreign` schemas and the Lua foreign-script link share the constructor, so one check covers all three. Full-width separators (U+FF0F, U+FF0E) are valid UTF-8 and stay literal names, as before.

Root cause of the `__stack_chk_fail`. Not a Xapiand stack buffer: the lldb stack of the abort (`.scratch/gpfix/lldb.out`) runs `DatabaseWALWriterThread` -> `DatabaseWAL` open -> WAL error -> `quarantine_files` (the `fs` library) -> `L_ERR("Directory {} not created ...", quarantine_path, ...)` -> `FormattedMsg` -> `vformat_to`. With the NUL path the template (`".."` plus NULs) made `mkdtemp` fail, and the log call took `quarantine_path`, a `char[PATH_MAX + 1]`, as a format argument. Findings, each reproduced:

1. Apple's libc++ formats a `char[N]` argument as all N bytes and aborts in `std::vformat_to(back_inserter(string), ...)` (canary check) for such a 1025-byte array; a stand-alone 12-line program aborts the same way (`.scratch/gpfix/fmt5.cc`, mode 1; llvm@22's libc++ formats it as a C string and does not). So on macOS any failure to make a quarantine directory (not only a NUL) aborted the node.
2. `strncpy(quarantine_path, ..., PATH_MAX)` leaves the array unterminated for a path of `PATH_MAX` bytes or more, and `build_path_index`/`std::string(quarantine_path)` then read past it: AddressSanitizer reports a stack-buffer-overflow READ of size 1171 in `quarantine_files` (fs test, below).

The ASan node run of `PUT /..%00/doc` (build without the NUL check, llvm@22) does not abort (200-series answer, "WAL ERROR in '..\x00/'" in the log): ASan sees only finding 2, and only when the byte after the array is nonzero, which a clean stack hides. Fixed at the source: `quarantine_files` builds the template in a `std::string` (`fs:291842c`, local branch `fix/quarantine-path-buffer` of `Kronuz/fs` off the pinned `11067ed`, not pushed). Regression tests: in `fs`, `test_quarantine_files` (ASan fails without the fix with the dirtied stack; passes with it); in Xapiand `tests/test_quarantine.cc`, which logs through the real logger (aborts on the pinned `fs` on this host, passes with the fix).

### GET / and the node's own routes

Master's `4ec3d6d03` made `Endpoint("")` (the base) a 400, and `GET /` built that Endpoint (the root used to be an index named "" that was allowed to be missing). The router now answers the routes that name no index without an Endpoint: `GET /`, `HEAD /` (200), `GET /:info` (the same as `GET /`), `GET /:metrics`, `GET /.name` (`is_node_route`, `node_info` in `search_views.cc`). `PUT /`, `POST /` and a search still answer 400 (`DELETE /` 405). `retrieve_database` lost its `is_root` branches (never reached with an index). Docs E2E: `reference-guide/api.md` "The Node" (`GET /`, `GET /.server.name`, `PUT /`), unit test `tests/test_root_routes.cc`, and the `harness/path_boundary_check.sh` rows.

### Clean 400s for malformed names

The same validator: backslash, control bytes, overlong UTF-8 (`%c0%af`, `%c0%ae%c0%ae`), surrogates, truncated sequences answer 400 (they were 500 with `"manifest: magic"` or `"lock journal directory"`). `.xapiand/node` (a reserved name, `dot-store-node` in the reviewer's matrix) still answers 500: not touched.

### The newman preflight

`harness/e2e_capture.sh` checks newman's major version (6 or later) on every run, not only for `SCHEME=https`, and says how to install one beside the work (`npm i --prefix DIR newman@6.2.3`); the host's global 4.5.0 is untouched. The proof runs use the pinned install at `~/.scratch/newman6`.

### Proof

| | Release | ASan+UBSan |
|---|---|---|
| ctest | 34 of 34 | 34 of 34 |
| docs E2E, 374 requests | 644 of 644 (641 before, +3 new) | 644 of 644, no sanitizer line in the node log |
| `graceful_shutdown_check`, `path_boundary_check` (now 143 ok lines), `cli_roundtrip_check` | rc 0 | rc 0 (143 ok lines) |
| the reviewer's probes (`nul_probe`, `nul_probe2` with and without `--shards 1`, `nul_crash`, `probe_http`, `path_boundary_check`), rerun on the fixed Release binary, outputs `*-fixed.out` | `..%00` answers 400 everywhere, the parent directory's files are unchanged, `nul_crash` exits with the node alive, `probe_http` 0 escapes and no 500 but `.xapiand/node` | |

- Fails without the fix:
  - `Endpoint` check disabled: `test_endpoint` 37 of 144 checks fail, `test_lua` (a foreign link `..%00/scripts/doc`) aborts on an uncaught `engine::Error`.
  - `GET /` and `GET /.server.name` against the reviewer's pre-fix Release binary: the docs E2E fails exactly those 2 assertions (642 of 644).
  - `quarantine_files` with the pinned `fs`: `xapiand_test_quarantine` aborts (SIGABRT, `__stack_chk_fail` in `__vformat_to`); with the fix 5 of 5. In `fs`, the same test under ASan reports the stack-buffer-overflow without the fix.
  - `test_root_routes` does not compile without `is_node_route`; the mutant below fails it.
- Mutants (each killed): `index_path::valid` accepting the overlong `E0 80..9F` (2 of 144 `test_endpoint` checks fail; `fuzz_index_path` aborts within 30 s); `is_node_route` returning true (10 of 41 `test_root_routes` checks fail).
- Fuzz: `tests/fuzz_index_path.cc` (libFuzzer, `-DFUZZ=ON`) checks `valid` against a second decoder that works from the code point's value; 31.9 million runs in 91 s under ASan+UBSan, no difference. The docs list it with the other two targets.
- A bug of mine, found by the probe run: removing the only `co_await` from `metrics_view` made it a plain function with no return (a trap, SIGILL, on `GET /:metrics`); it ends with `co_return` now. `path_boundary_check.sh` and the E2E cover it.

## Done (commits)

- `Xapiand:6c820df05` the owed rows: allowlist, `read_uuid` and `init_database` deleted, CI step, the tree test.
- `Xapiand:4e38c40a6` the dead `thread.hh` and `threadpool.hh` includes.
- `Xapiand:c38c35ffb` D16 restated (design, control-plane note, `options.md`, changelog, D14).
- `Xapiand:9fcdf2dd1` absolute index paths refused (`Endpoint`, tests, harness, changelog).
- `Xapiand:a9efae7f9` `wal_view` reads the log in a query chore; the `wal` rule of `async_check`.
- `Xapiand:a8167224c` SR1: NUL and unholdable bytes refused in `Endpoint` (`index_path::valid`, tests, `fuzz_index_path`); the clean 400s.
- `Xapiand:bfd3431ff` the node's own routes answer without an index Endpoint (`GET /` and the rest), docs E2E, `test_root_routes`.
- `Xapiand:788c3cc22` `test_quarantine` through the real logger (needs `fs:291842c` pinned).
- `Xapiand:5fbad2b76` newman 6 preflight; `path_boundary_check.sh` rows for SR1 and the node routes.
- `Xapiand:5ecbf190b` changelog and ledger.
- `fs:291842c` (local branch `fix/quarantine-path-buffer` of `Kronuz/fs`, worktree `fs-wt/quarantine-fix`, not pushed) `quarantine_files` builds its template in a `std::string`.

## Next

- The lead: push `Kronuz/fs` branch `fix/quarantine-path-buffer` (`fs:291842c`, off the pinned `11067ed`; its own `fs_test` passes under ASan), then set the `fs` `GIT_TAG` in `CMakeLists.txt` to the pushed commit. Until then `xapiand_test_quarantine` aborts on the pinned `fs` (every other test passes); the proofs above used `-DFETCHCONTENT_SOURCE_DIR_FS` pointing at the branch.
- The security reviewer: re-check SR1 (`nul_crash.py`, the `nul_probe*` and `probe_http.py` outputs are in `.scratch/sec-endpoint/*-fixed.out`) and the shared constructor.
- Review (the lead's skim); then the lead merges and X7.
- Follow-up (not done, scope): `:wal` still builds the whole log as one `MsgPack` (unbounded memory for a large log); it also needs a shard path (`/idx/.__N/:wal`), an index of several shards answers 400. The WAL stays until 7b.
- Follow-up (not done): `PUT /.xapiand/node` answers 500 with an internal message (a reserved name).
- Follow-up (Apple libc++): any other `char[N]` passed straight to a log call formats as N bytes there; the `fs` fix covers the one that aborted.

## Blocked on

Nothing.
