# reactor-open-files: journal

State: waiting on jobs 261120-233922-3e0cfb 261120-233941-918f30 261120-233945-e2d203
Quest: .campaign/xapiand/quests/reactor-open-files.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1/A2 done at reactor:303febb, jobs queued; branch pushed for CI. A3 blocked on the file set.

## Next

1. Read the three jobs; CI green on all platforms (A4).
2. A3 once the lead widens the file set (see Blocked on).

## Blocked on

The lead: A3 (`seconds_to_expiry`, reviewer F6) lives in `reactor_tls.h`, tested in
`test/tls_test.cc`; neither is in this quest's file set (reactor.h, test/limits_test.cc,
test/test.cc, CMakeLists.txt). Options:

1. Add `reactor_tls.h`, `test/tls_test.cc`, `docs/API.md`, `CHANGELOG.md` to the file set. Fix:
   return `std::optional<long>`, `nullopt` for no certificate or an unreadable notAfter, 0 for
   "expires this second". A signature change, but no caller outside reactor: Xapiand uses its own
   `tls_seconds_to_expiry()` (src/tls.cc:73-78), with no call to `reactor::tls::seconds_to_expiry`.
2. Keep the signature, return `LONG_MIN` when unreadable (reads as long expired, so the warning
   fires): no signature change, but a sentinel.
3. Drop A3 here (the reviewer rated F6 a nit, "keep").

Recommendation: 1.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | limits_test [T] FreeBSD int64 targets and [M] real SIZE_MAX: 261120-233941-918f30 at reactor:303febb; `--fails-without` at the unclamped-cast mutant reactor:927579c (local branch mutant/reactor-open-files-a1, not pushed): 261120-233945-e2d203 |
| A2 | limits_test [C] macOS cap (host: hard unlimited, kern.maxfilesperproc 1048576) and [S] refused setrlimit: 261120-233941-918f30 |
| A3 | blocked: file set |
| A4 | CI at reactor:303febb (pushed) |
| suite | 261120-233922-3e0cfb |

## Result

reactor:303febb. `raise_open_files` computes its ask with `detail::open_files_target<Limit>`,
generic over the limit type: it clamps `wanted` to the type's max (FreeBSD's signed rlim_t no longer
goes negative), the hard limit and the per-process cap. `detail::raise_open_files(wanted,
set_limit)` takes setrlimit as a seam. The public `raise_open_files(std::size_t)` signature is
unchanged; Xapiand has no caller yet (no `raise_open_files` in its src/).

New `test/limits_test.cc` (ctest `reactor_limits`): [T] targets on FreeBSD, macOS and Linux limit
types with values; [R] the real raise (moved from reactor_test); [M] SIZE_MAX to the hard limit;
[C] the macOS cap at maxfilesperproc + 1; [S] a refused setrlimit returns the soft 1024 in force.

## Rulings I made

- The planner is generic over the limit type, so FreeBSD's signed arithmetic is tested on every
  host, not only in the FreeBSD CI job; the real SIZE_MAX call covers each CI platform.
- Clamping to the hard limit treats RLIM_INFINITY as the largest valid value (true on Linux, macOS,
  FreeBSD): SIZE_MAX under an infinite hard limit asks for RLIM_INFINITY, bounded by the FreeBSD
  kernel's clamp or the macOS cap.
- The refused path is tested through a setter seam in `detail`, not by interposing `setrlimit`:
  portable, no per-libc prototypes.
- The [R] cases moved from test/test.cc to limits_test.cc.
- The doc comment says FreeBSD also caps at kern.maxfilesperproc (its kernel clamps in setrlimit).
  docs/API.md:96 still says only macOS's cap; it is outside the file set, and still true.
