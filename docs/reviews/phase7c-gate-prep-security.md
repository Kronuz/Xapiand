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
