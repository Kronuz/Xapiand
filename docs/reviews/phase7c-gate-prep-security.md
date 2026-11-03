---
title: "phase7c gate-prep — security review"
---

Branch `phase7c/gate-prep` (worktree `Xapiand-wt/gate-prep`), HEAD `Xapiand:fdc82abfe5` on top of
master `Xapiand:81c311799`. Scope: the index-path boundary (`Endpoint`, `src/endpoint.cc`),
commit `Xapiand:9fcdf2dd1` ("Refuse absolute index paths outside the data directory") and
`Xapiand:17d0a7077` ("Make the path boundary check match the router"), per
`docs/status/phase7c-gate-prep.md`.

Threat model (from the task): trusted cluster, untrusted HTTP client input. An index path from a
client (URL path, percent-encoded segments, `_foreign` schema references, Lua foreign-script
links, the CLI's `--restore`/`--dump`) must never resolve to a directory outside the node's data
directory, for create, read, write or delete.

Build: Release, `build-sec/` in the worktree (`cmake -S . -B build-sec -DCMAKE_BUILD_TYPE=Release
-DBUILD_TESTS=ON ...`), under `quiet light`. A second binary, `.scratch/sec-endpoint/xapiand-prefix`,
is the identical tree with only `src/endpoint.cc` swapped to its state at `9fcdf2dd1^` (the
pre-fix check), used for a differential test; `src/endpoint.cc` was restored immediately after
that one build, confirmed via `git diff --stat`.

Probes, harnesses and mutants (kept, `.scratch/sec-endpoint/`):

- `sec_lib.py` — a small HTTP-raw-socket node launcher shared by the probes below.
- `probe_http.py` / `probe_http.out` — ~75 percent-encoding, Unicode, overlong-UTF-8, backslash,
  colon, wildcard and long-path vectors against `PUT`/`GET`/`DELETE`, diffed against a canary
  directory outside the data dir, plus the same vectors replayed against `:search`, `:dump`,
  `:restore`, `:commit`, `:info`, `:count`, `:check`, `:wal`.
- `nul_probe.py` / `nul_probe.out`, `nul_probe2.py` / `nul_probe2.out` /
  `nul_probe2-shards1.out` — the `%00` family specifically (plain, in a `_foreign` link via JSON
  and via urldecode).
- `nul_crash.py` / `nul_crash.out` — reproduces the crash under `--shards 1` and reports the exit
  status.
- `np.cc` / `np` (and `fuzz_norm.py`) — the shipped `fs::normalize_path` extracted into a tiny
  CLI, differentially fuzzed against Python's `posixpath.normpath` (900000 cases, 0 diffs) and
  used directly to confirm the NUL-byte finding below.
- `run-lldb/`, `run-lldb2/`, `run-lldb3/` — lldb sessions capturing the abort backtrace.
- `path_boundary_check.out` — the branch's own `harness/path_boundary_check.sh` run against
  `build-sec/bin/xapiand` (rc 0, 77 "ok" lines, matching the ledger).
- `e2e-newman4/`, `e2e-newman4.log`, `e2e-newman6/`, `e2e-newman6.log`, `newmanbin/` — the lead's
  newman question, addressed under "Re-check of the lead's questions" below.
- `old/endpoint.cc.pre-fix` — `git show 9fcdf2dd1^:src/endpoint.cc`, the file used for the
  differential build.

## Findings

### SR1 (Critical) — a NUL byte in an index path defeats the boundary check and crashes the node

**Attacker:** any client that can reach the HTTP API (no auth gate was found in front of the
index routes — `grep` for an auth check in `src/server/search_views.cc` found none besides the
`opts.admin_commands` gate on two root diagnostic views). Per the task's own threat model, this
is the party the fix exists to defend against.

**What they gain:** a directory create/open/lock operation that the fix's own check reports as
"strictly inside the data directory" is actually performed by the OS one directory level above
it, and, in at least one supported configuration (`--shards 1`), a single crafted request aborts
the node (denial of service).

**Mechanism.** `Endpoint::Endpoint` (`src/endpoint.cc:236`) normalizes the path and then checks:

```cpp
path = normalize_path(_path);
if (path.size() <= Endpoint::cwd.size() || !strings::startswith(path, Endpoint::cwd)) {
    THROW(ClientError, "Index path {} is empty or outside the data directory", repr(original_uri));
}
```

Both `normalize_path` and the check operate on a `std::string`/`std::string_view`, which is
length-based and NUL-agnostic. The shipped `fs::normalize_path` (`fs.cc:484`) only recognizes a
path component as `".."` when it is exactly two bytes (`size == 2 && name[1] == '.'`); a
component of `".." + NUL` is three bytes, so it is kept as an ordinary (if unusual) literal name,
one directory level *inside* whatever came before it. Confirmed directly against the shipped
function, extracted verbatim into `.scratch/sec-endpoint/np.cc`:

```
$ printf '..\x00\n' | ./np   # 3 input bytes: '.' '.' NUL
..\x00                       # unchanged — kept as a literal 3-byte name, not collapsed
```

So `Endpoint("..%00/doc")` (`urldecode` turns `%00` into a real NUL byte) normalizes to
`cwd + "/..\0"`, which is longer than `cwd` and starts with it — the check **passes**. But every
downstream POSIX call that takes this string as a `const char*` (`opendir`, `mkdir`, `flock`,
...) stops at the embedded NUL and sees only `cwd/..` — the data directory's own parent.

**Confirmed live**, two ways:

1. Default config (5 shards): `PUT /..%00/doc` on a clean node answers `500 lock journal
   directory: Resource temporarily unavailable` (`nul_probe2.out`) — the node is trying to flock
   a directory it does not expect to be contended, consistent with concurrently resolving shards
   racing to flock the *same real, external* directory (the data dir's parent) rather than five
   distinct, freshly-created subdirectories of their own.
2. `--shards 1`: the same request aborts the process. lldb backtrace
   (`run-lldb3/lldb.out`, also reproduced in `run-lldb/` and `run-lldb2/`):

   ```
   thread #23, name = 'Xapiand:WL05', stop reason = signal SIGABRT
     frame #3: libsystem_c.dylib`__stack_chk_fail + 102
     frame #4: xapiand`...__vformat_to<...>(...)
   ```

   `__stack_chk_fail` — the compiler's stack-protector catches a stack-buffer overflow inside a
   `std::format`/log call that is building a message out of the escaped (and apparently
   unexpected) path or its directory contents. The Release build's missing frame pointers (no
   `-fno-omit-frame-pointer`) keep the backtrace from going further than the formatting
   frame; the canary stops this from being more than a crash, but it is a crash reachable with
   one HTTP request, deterministic and repeated identically three times
   (`nul_crash.py`, exit status `-6`/SIGABRT; `run-lldb*`).

**Bound on the escape.** Working through `normalize_path`'s algorithm: every *clean* (NUL-free)
`".."` component in the path is correctly collapsed and clamped to the data directory by the
existing logic (both in the checker and, lexically, in the OS) exactly as the already-tested
plain-`".."` cases show. The divergence is strictly local to the one component that fuses real
dot-dot bytes with the NUL; nothing after the first NUL in the whole string is ever seen by the
OS (`c_str()` truncates the *entire* string at the first NUL, not per-component), so an attacker
cannot chain multiple hidden pops in one request. The worst case is exactly one level above the
data directory — still a clear violation of "must never resolve to a directory outside the
node's data directory", and, depending on deployment (siblings under a shared parent, e.g.
multiple nodes/tenants under one `/var/db/`), that parent is not nothing.

**Shared code path.** `Endpoint(foreign_path)` for a `_foreign` schema reference
(`src/database/schemas_lru.cc:504,606,647`) and for a Lua foreign-script link
(`src/lua/processor.cc:522`) both go through the exact same constructor and check; both are
urldecoded client input and both reproduce the same symptom class (`nul_probe2.out`: a `_foreign`
schema naming `..%00/sch`, and one naming the JSON-escaped `..\u0000/sch`, both error the same
way as the direct route). I did not separately chase a crash through these two routes given the
shared root cause and the time box; they inherit the fix once `Endpoint`'s own check does.

**This blocks landing.** It is a direct violation of the invariant the branch exists to add, it
is remotely triggerable, and it has a crash path in a supported configuration.

Recommendation: reject any path containing a NUL byte (or any other byte a POSIX path component
cannot legitimately contain) before or during normalization, in `Endpoint`'s constructor — not
only after the prefix check. `test_endpoint.cc` and `path_boundary_check.sh` have no `%00` case;
add one for each entry point (direct index route, `_foreign`, Lua link).

### Low — malformed-but-"inside" names are accepted and fail later, with an internal-error message

Several vectors that stay lexically inside the data directory (backslash, full-width Unicode
slash/dot, raw `:`, `*`, `,`, control characters, overlong UTF-8 `%c0%af`/`%c0%ae%c0%ae`) are
accepted as literal directory names by `Endpoint` and only fail later, deep in storage/manifest
code, surfacing as a `500` with an internal message (`"manifest: magic"`,
`"lock journal directory: ..."`) rather than a clean `400` (`probe_http.out`: `nul-trunc`,
`nul-abs`, `overlong-slash`, `overlong-dot`). None of these escaped the data directory in
testing (confirmed via the before/after directory snapshot in `probe_http.py`) — this is a
robustness/error-hygiene gap, not a boundary bypass. Non-blocking; worth a follow-up to reject
control bytes and other characters the storage layer cannot round-trip, with a `400`, before
they reach storage.

### Symlinks — not a gap under this threat model

`grep -rn symlink` across `src/` and every vendored library actually linked into `xapiand`
(`build-sec/_deps/*-src`) found no call outside test code. No client-reachable API (HTTP, CLI,
`_foreign`, Lua) creates a symlink. Under the stated threat model (remote/API client input;
local file access is a separate, out-of-scope attacker per the task), a symlink planted inside
the data directory can only come from someone who already has local file access to that
directory — the developer's deferral of the symlink case is correct, not a gap.

### `.scratch/sec-endpoint/probe_http.out` — the rest of the matrix

Everything else probed (percent-encoded `..`, double/triple percent-encoding, mixed case,
`%2e%2e`, overlong `..%2f..%2f`, literal doubled/tripled leading slashes, `%2f` at the front, a
`_foreign` schema naming an absolute path, long paths up to several KB, UUID-shaped segments,
`.xapiand`/`.__N` reserved names) stayed inside the data directory or was refused with `400`,
matching the ledger's claims. The branch's own `harness/path_boundary_check.sh` also passes
clean against `build-sec/bin/xapiand` (`path_boundary_check.out`, rc 0, 77 ok).

## Re-check of the lead's questions

**"`GET /` returns 400; list every route the fix wrongly refuses."** Confirmed the symptom:
`GET /`, `HEAD /`, `GET /:info`, `GET /:metrics` and `GET /.nodes` all answer `400 Index path ''
is empty or outside the data directory` (confirmed both with a raw socket and with plain `curl`,
ruling out a harness artifact). **But this is not caused by the commit under review.** I built a
second binary (`xapiand-prefix`) identical in every other respect but with `src/endpoint.cc`
reverted to its state one commit before `9fcdf2dd1` (the pre-fix, relative-only check) and ran
the same requests: identical `400`, identical message, on every one of them. The reason is
arithmetic, not the `relative &&` guard the commit removed: `expand_paths()` gives a bare root
request a single empty-string path, `Endpoint("")` is relative, so **both** the old and the new
check prepend `cwd` and then reject because the normalized result is no longer longer than `cwd`
— the `relative &&` guard the commit dropped was true in both cases, so the outcome is the same
either side of the diff. **So: zero routes are newly or wrongly refused by this fix; this
specific symptom is a pre-existing condition on master, unrelated to the Endpoint check this
branch touches**, and is not a blocker for this review (I cannot fix it inside this diff, and it
is not a path-boundary issue). It is real, though, and worth a separate bug: the active
`[Unreleased]` changelog documents `GET /`'s `versions` and (until removed) `nodes` keys as a
currently-live response shape, so this is a regression against documented behavior somewhere on
master before `phase7c/gate-prep` started, not stale history.

**"Newman reportedly exits silently under Node 26; check the E2E actually ran."** The host's
`newman` on `$PATH` is `4.5.0` (`newman --version`), not `newman 6.2.3` as the ledger's proof
table says. I replayed the harness exactly as the ledger describes it, against this review's own
`build-sec/bin/xapiand`, with that exact newman: it does **not** exit silently — it exits `1`
loudly, and `e2e_assert.py` reports `pass 0, fail 641` with explicit errors
(`e2e-newman4.log`, `e2e-newman4/report.json`: every request shows `response: HTTP None`, i.e.
newman 4's older variable substitution mangles the generated collection's templating, consistent
with the harness's own header comment, "newman 4.5 fails every request"). I then installed
`newman@6.2.3` via `npx` into a throwaway `PATH` shim (`newmanbin/newman`, no change to the
host's global `newman`) and reran the identical harness against the identical binary:
`e2e-newman6.log` / `e2e-newman6/report.json` — newman exits `0`, `e2e_assert.py` reports
`requests=371 assertions=641 (pass 641, fail 0)`, `GREEN`. **So the ledger's E2E proof is
genuine and reproducible** — the gap is that this host's globally-installed `newman` has
regressed to 4.5 since that proof was taken (plausibly from an unrelated `brew install awscli`
hold I saw running on this same host via `quiet list`, which can touch the global Node/npm
toolchain as a side effect; not confirmed further). Non-blocking for this review, but worth a
tooling note to the lead/gate owner: `harness/e2e_capture.sh` only enforces the newman-6
requirement for `SCHEME=https`; pin or preflight-check the newman major version
unconditionally, since a silently-wrong-but-loud failure here could otherwise be mistaken for a
real regression in the branch.

## Verdict

**APPROVE WITH CONDITIONS.**

Blocking:
- **SR1 (Critical).** A NUL byte (`%00`) in an index path is not recognized by `normalize_path`'s
  `".."`-detection, so `Endpoint`'s boundary check validates a different string than the one
  POSIX calls act on; confirmed to redirect a create/lock operation one directory level outside
  the data directory and to abort the node (`SIGABRT` via the stack-protector) under `--shards
  1`. Close by rejecting NUL (and ideally any other byte a path component cannot legitimately
  hold) in `Endpoint`'s constructor, before the prefix check, and add a `%00` case to
  `test_endpoint.cc` and `path_boundary_check.sh` for the direct route, `_foreign`, and the Lua
  foreign-script link.

Non-blocking (file separately, do not hold the gate):
- The pre-existing `GET /`/`:info`/`:metrics`/`.nodes` 400 (not caused by this branch; contradicts
  the active changelog's documented `GET /` response shape).
- The host's default `newman` (4.5.0) will loudly fail the E2E harness; pin/preflight the newman
  major version in the harness rather than relying on a header comment.
- Low: malformed-but-inside names (backslash, full-width Unicode separators, control bytes,
  overlong UTF-8) surface as `500`s with internal error text instead of a clean `400`.

Conditions to re-check: SR1's fix, with the two added test cases (direct route and `_foreign`/Lua),
and a rerun of `nul_crash.py` and `np` against the fixed binary to confirm the crash and the
differential both close.

Not run: ASan/UBSan build (not needed — the crash is a stack-protector abort on untrusted input,
not a memory-safety question the sanitizers would clarify further; a Release build was enough to
reproduce it deterministically three times). TLS/cert probes (out of scope for this diff — no TLS
code touched). CLI `--dump`/`--restore` NUL case (CLI args are operator input, not the client
threat model this review scopes to; not deeply probed beyond noting the shared `Endpoint`
code path). Fuzzing beyond the `normalize_path` differential (900000 cases) and the ~75-vector
live HTTP probe — no corpus-based fuzz target exists for this code and the ledger's own "Proof"
section notes none was owed.

## Re-check

Scope: SR1, the 500-on-malformed-name note and the `GET /` routes, against `Xapiand:a8167224c`, `Xapiand:bfd3431ff`, `Xapiand:5fbad2b76` and `fs:291842c` (pinned by `Xapiand:fe0af3000`). Build: Release, `build-sec/` reconfigured with `FETCHCONTENT_SOURCE_DIR_FS=~/code/Kronuz/fs-wt/quarantine-fix` (fs at `fs:291842c`), HEAD `Xapiand:fe0af3000`; servers on ports 18951-18967 with data under `.scratch/sec-endpoint/run-*`, none left running. The unfixed `build-sec` binary was kept as `.scratch/sec-endpoint/xapiand-unfixed-sr1` for a differential.

### Rulings

- **SR1 (NUL in an index path): CLOSED.** `src/endpoint.cc:237-239` calls `index_path::valid(_path)` (`src/index_path.h:33-71`) on the string that is about to become the path, before the relative-prefix and `normalize_path` steps, for every protocol branch (`file://..\0/doc` included, since `_path` is assembled at `endpoint.cc:213-217` first). `valid` refuses NUL, C0, DEL, backslash and ill-formed UTF-8 (overlongs through the E0/F0 lower bounds, surrogates through the ED upper bound, above U+10FFFF through F4, C0/C1 and F5+ lead bytes, truncated sequences). Reruns against the fixed binary: `nul_probe.py` (`nul_probe-recheck.out`) every `..%00` vector is 400 and the parent directory holds only the planted files, unchanged; `nul_probe2.py` default and `--shards 1` (`nul_probe2*-recheck.out`) 400 on every route, the JSON-escaped `..\u0000` foreign schema links (204, stored lazily) but its first write is 400 and nothing appears above the data dir, the `%00` foreign link is refused at link time; `nul_crash.py --shards 1` answers 400 and the process stays up (`nul_crash-recheck.out`, exit status `None`; it was `-6`). `probe_http.py`: 0 escapes, and against the original run only the intended rows changed (backslash 204/200/204 became 400; nul-trunc, nul-abs, nul-dotdot, overlong-slash, overlong-dot 500 became 400). The branch's `harness/path_boundary_check.sh` passes (143 ok, rc 0; it was 77) and fails on the unfixed binary (NUL, `%5C`, `%01` cases 500/204 where 400 is wanted), so it is sensitive. Reverting the check (removing the three lines at `endpoint.cc:237-239`) fails `test_endpoint` (37 of 144 checks, `tests/test_endpoint.cc:132`), aborts `test_lua` (uncaught `engine::Error` in the new foreign-link test) and fails the harness (53 FAILs); restored, 144 and 730 checks pass. The crash's root cause is closed separately below.
- **SR1 root cause, `fs:291842c`: CLOSED.** `fs.cc:134-147` builds the template in a `std::string` and calls `io::mkdtemp(quarantine_path.data())`; the log call formats the string, not a 1025-byte array. `xapiand_test_quarantine` (`tests/test_quarantine.cc`) passes (5 checks) with the fix and aborts (rc 134) when the fix is reverted (fs copy at `11067ed` in `.scratch/sec-endpoint/fs-prefix`, restored and rebuilt after).
- **500 on a malformed name (Low note): CLOSED for malformed input.** Backslash, control bytes, DEL, NUL, overlong/surrogate/out-of-range/truncated UTF-8 all answer `400 Index path ... holds a byte a name cannot hold` on PUT/GET/DELETE and on `:search`, `:info`, `:commit` (`recheck-fixed.out`, 181 two-hundreds among the encoded-variant vectors, none of which decodes to a refused byte in a path position). Residual, not a regression and not a boundary issue: three well-formed code points the host filesystem refuses (U+D7FF `%ed%9f%bf`, U+FFFE `%ef%bf%be`, U+10FFFF `%f4%8f%bf%bf`) still answer `500 filesystem error: in create_directory` on this APFS host, identical before and after the fix.
- **`GET /` routes: CLOSED.** `src/server/search_views.cc:768-772` (`is_node_route`: no path part and an empty id) and `:775-788` (`node_info`) back `info_view` (`:1220`), `database_exists_view` (`:1278`) and `retrieve_database_view` (`:1381`). Live: `GET /`, `HEAD /`, `GET /:info`, `GET /:metrics`, `GET /.name`, `/.server.name`, `/.state`, `/.options` answer 200 with the documented shape; `PUT /` and `POST /` still answer 400, `DELETE /` 405, `GET /:search` 400. `GET /:metrics` returns the text and the node stays up (the `co_return` fix). Docs E2E with newman 6.2.3 against the fixed binary: `requests=374 assertions=644 (pass 644, fail 0)`, GREEN (the original run was 371/641). Mutants of `is_node_route` all die in `test_root_routes`: `return false` (6 of 41 fail) and the harness (`GET /`, `HEAD /`, `/:info`, `/.name`, `/.state` 400), `has_pth` alone, `get_id` alone (3 fail each), `return true` (10 fail).
- **newman preflight, `Xapiand:5fbad2b76`: CLOSED.** `harness/e2e_capture.sh:49-54` checks the major version on every run: with the host's newman 4.5.0 first on `PATH` it prints "needs newman 6 or later (found '4.5.0')" and exits 1 before starting a node; with `newmanbin` (6.2.3) first it runs.

### New mechanisms, probed

- **Legitimate names.** 106 PUT+GET vectors (`recheck_probe.py`, `recheck-fixed.out` against `recheck-unfixed.out`): UTF-8 (percent-encoded `café`, CJK, an emoji), dots (`a.b`, `x..y`, `..x`, `x..`, `...`, `.hidden`), dashes, underscores, spaces, `~`, `@`, `:`, `+`, `'!$=;&()`, uppercase, a 10-deep path, UUID-shaped, the full-width slash and dots, U+2025, U+0080, U+E000, U+FFFD, and the docs' own names (`.xapiand/indexes`, `bank/accounts`, `twitter/tweet`, `my_index/part-1`): no vector differs between the fixed and unfixed binaries, so no legitimate name is newly refused. The `%00`, `%2500`, `%252e%252e`, `%255c` double-encoded forms create literal directories of those names inside the data dir (one decode only, no control byte or backslash in any name under `data/`).
- **Node route versus index, both directions.** 59 paths by 5 methods, fixed against unfixed: only GET/HEAD (and POST `/:info`) on paths that fold to the root change from 400 to 200 (`/`, `//`, `/./`, `/..`, `/a/..`, `/a/b/../..`, `/:info`, `/:metrics`, `/.name...`, a selector with a NUL or `{}`/`[name]`). Every path with an index or document part (`/idx`, `/idx/`, `/idx/:info`, `/a,b`, `/idx@x`, `/@x`, `/*`) and every encoded form (`/%2e%2e`, `/%2e`, `/%2f`, `/%2F`, `/%3Ainfo`, `/%20`, `/%00`) is unchanged. No mutating method (PUT, DELETE, POST other than `:info`) changed. A node route never builds an Endpoint, and the selector is read from the node's own map, so the 33 `GET /.<refused byte>` rows that answer 200 name no path.
- **Percent-encoded variants of the newly refused bytes.** 55 encodings (`%00`, `%01`..`%1f`, `%7f`, `%5c`/`%5C`, overlongs in both cases, surrogates, `%f4%90...`, `%ff`, `%80`, `%00%00`, `%2e%2e%00`, `%u0000`, a bare `%`, `%zz`) in four path positions plus `:search`, `:info`, the query string and a selector: every one that decodes once to a refused byte in an index position is 400; `%2500`, `%252500` and `%25%30%30` are stored as the literal text (one decode, never two); `%u0000`, `%0`, `%`, `%zz` are literal names. 0 entries outside `data/` in every run (`recheck-fixed.out`: 0 ESCAPE lines; the unfixed binary creates `manifest-1` and `meta-1.kv` above the data dir).

### Not blocking, for the lead

- `HEAD /:stats`, `/:schema`, `/:nope` (any command) at the root answer 200 through `database_exists_view`; `HEAD /idx/:info` is the existing 400. Harmless, a little loose.
- A raw (not percent-encoded) non-ASCII byte in the request target gets no response on either binary (status 0 in `recheck-fixed.out`); percent-encoded UTF-8 works. Pre-existing HTTP parser behavior, not caused by this branch.
- `test_quarantine` leaves `quarantine_ok/` and `quarantine_denied/` (mode 0500) in its working directory; run from the repo root they show up as untracked.
- The foreign-schema JSON-NUL link is stored (204) and refused on first use (400) rather than at link time; the `%00` form is refused at link time. No escape either way.

Not run: ASan/UBSan build, the libFuzzer target `fuzz_index_path` (the Homebrew LLVM fuzzer build), Linux/FreeBSD, TLS, CLI `--dump`/`--restore` beyond the branch harness's own cases (they pass), a mutant of the `metrics_view` `co_return` (covered live by `GET /:metrics` and the harness).
