# endpoint-uuid review: correctness

Commit: Xapiand:b1667d00f810d7227c80e72e3a7ecfe3cb7ecf69
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-180245-ed73df (test_endpoint, test_coverage_core_endpoint, test_endpoint_uuid passed) and
  suite 261119-180250-f48c78, both at 3b821c009f6c. The merge to b1667d00f810 touches only
  `.campaign/`, `harness/` and `docs/design/light-share.md`, so both still hold.
- Mutants 261119-180304-eeab13 (A2) and 261119-180306-f79912 (A3) fail without the fix. The first
  mutant run, 261119-164341-1d09a8, also shows what the old normalization did under partition: PUTs
  answered 500 "Couldn't open storage directory", and rebuilt paths doubled
  (`4526/10c3/4526/10c3/...`).

## What I checked myself

The scripts are in `.scratch/xapiand/review-endpoint-uuid-correctness/`. `run.sh default|guidurn` copies
`test_zz_review_endpoint_prop.cc` into the job's tests/ for the run and removes it on exit.

- Q1, idempotence. 261119-194145-1bc386 ran on the default build (vanilla, encoded) and printed
  "OK: 633600 properties held". 261119-194153-e97f59 ran with UUID_GUID and UUID_URN on and held
  1267200 properties; there test_endpoint's own matrix passed 162 checks in the guid and urn
  reprs too. The probe covers 1,800 UUIDs: random v1, v4 and v6 hex, plus minted v1 compact, v1
  non-compact and v6. Each repr ran with partition and legacy both off and on. The probe checks
  three things:
  - P1: `Endpoint(Endpoint(x).path).path` equals `Endpoint(x).path`, also when the path is given
    absolute under cwd.
  - P2: lower case, upper case, `{…}`, `urn:uuid:…` and the mode's own text of one UUID all give
    one path.
  - Both properties hold in 8 nesting contexts (`idx/`, `/child`, `idx.`, `a/b/…/c`, `/.__1`,
    `.x`, `x.…/y.z`) and for two UUIDs joined by `/`, `.` and `;`. No property failed.
  - `--uuid compact` only affects minting (schema.cc:2796), not normalization, so it needs no row
    of its own.
- Q2, upgrade:
  - With partition off, nothing changes. `normalize` never returns a `/`, because the base59
    alphabet is alphanumeric. So `replace_segment` (endpoint.cc:156) falls through to the old
    resize-and-append.
  - With partition on, the new code differs from the old only when the buffer already ends with
    the UUID's partition directories (endpoint.cc:163). A name created through the released code
    never ended that way. Every partitioned vanilla, guid or urn index failed to write under the
    released code. Encoded-condensed indexes such as `~/oB/9w/ZE/F7pFNvHzvYbn` did write, and they
    get the same directory now. So no openable on-disk index moves.
  - The `:` change does move names. An index created as `a:b`, `a:b/c` or `urn:uuid:<u>` used to
    live in `a`, `a/c` or `urn`; those names now open new directories. The changelog entry states
    this (`a:b` opens `a:b`; the index created before lives in `a`). A `:` after the first segment
    was never cut, so those names are unaffected.
- Q3, port callers: none.
  - `src/` (with xapian excluded) has no replication or remote-protocol code.
  - The only producer of `xapiand://` is `Endpoint::to_string` (endpoint.cc:336). It writes the
    node name with no port, and nothing parses its output back.
  - The other rebuild sites pass bare paths: shard.cc:306/1368/1397/1898, pool.cc:895, and
    index_resolver_lru.cc:789, which passes the path plus a Node. HTTP paths go through
    wide_search.cc:117 as file paths.
  - `invalidate_settings(uri)` (index_resolver_lru.cc:828) has no callers.
- Q4, the pool test's directory: sound.
  - test_endpoint_uuid.cc:367-370 enters `uuid_pool_data` under the ctest working directory. The
    CMake `add_test` call sets no WORKING_DIRECTORY, so that is the build dir.
  - It is the last test `main` runs (line 395). The node tests change directory only in forked
    children, so no later test in the process inherits the directory.
  - Its directory names are unique in tests/, and it follows the convention of test_node.cc:434
    and test_loop_yield.cc:164.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | The new options.md sentence "With `partition`, a UUID's index lives two directories down, under its third and second groups" is true only for vanilla, guid and urn text. In the default encoded mode, every condensable UUID (every UUID Xapiand mints) lives four directories down, under its encoded characters: `a2d6e3b4-…` → `~/oB/9w/ZE/F7pFNvHzvYbn`. | probe 261119-194145-1bc386 layout lines; endpoint.cc encoded case of `normalize_and_partition` | Say "in the vanilla text, … ; an encoded UUID lives under its first encoded characters (`~/oB/9w/ZE/…`)". |
| N2 | no | nit | The released code under partition could leave empty, doubled directories from its failed PUTs (`4526/10c3/4526/10c3/<uuid>`; the old-normalization mutant's layout check found `4526/10c3/4526`). They hold no documents, since those PUTs answered 500. The new code reaches them only by their literal doubled name. | 261119-164341-1d09a8 (test_endpoint_uuid.cc:306 failing) | Optional: one changelog clause saying such directories can be deleted. |
| N3 | no | gap | Only the default build (vanilla, encoded) runs in the suite and CI. The guid and urn rows of test_endpoint's matrix run only when the build turns those options on. I ran them once here, and they pass. | 261119-194153-e97f59 | None needed for this quest. |

## Not run

- I did not run the released binary (alpha.9) against a real data directory. Q2 rests on the code
  diff and on the old-normalization mutant's log, not on an upgraded on-disk fixture.
- I did not check what the leftover doubled directories (N2) hold beyond the 500s their PUTs got.
- I did not check CI at b1667d00f810 (it was running), and I did no sanitizer or FreeBSD/arm64
  runs; those belong to CI.
