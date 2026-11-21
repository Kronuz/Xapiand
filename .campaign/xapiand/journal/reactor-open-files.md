# reactor-open-files: journal

State: waiting on jobs 261120-234534-1ad044 261120-234531-7c2c9a 261120-234540-8adc88 261120-233945-e2d203
Quest: .campaign/xapiand/quests/reactor-open-files.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All fixes at reactor:01db032 (pushed); jobs queued; CI running.

## Next

1. Read the jobs; CI green on all platforms (A4).

## Blocked on

Nothing. A3 unblocked by msg from the lead (file set widened; ruling: `std::optional<long>`).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | limits_test [T] FreeBSD int64 targets and [M] real SIZE_MAX: 261120-233941-918f30 at reactor:303febb, 261120-234540-8adc88 at reactor:01db032; `--fails-without` at the unclamped-cast mutant reactor:927579c (local branch mutant/reactor-open-files-a1, not pushed): 261120-233945-e2d203 |
| A2 | limits_test [C] macOS cap (host: hard unlimited, kern.maxfilesperproc 1048576) and [S] refused setrlimit: 261120-233941-918f30, 261120-234540-8adc88 |
| A3 | tls_test server_context (expiring now reads 0/-1, unreadable notAfter and no certificate read nullopt): 261120-234531-7c2c9a at reactor:01db032 |
| A4 | CI at reactor:01db032 (pushed) |
| suite | 261120-234534-1ad044 at reactor:01db032 |

## Result

reactor:303febb. `raise_open_files` computes its ask with `detail::open_files_target<Limit>`,
generic over the limit type: it clamps `wanted` to the type's max (FreeBSD's signed rlim_t no longer
goes negative), the hard limit and the per-process cap. `detail::raise_open_files(wanted,
set_limit)` takes setrlimit as a seam. The public `raise_open_files(std::size_t)` signature is
unchanged; Xapiand has no caller yet (no `raise_open_files` in its src/).

New `test/limits_test.cc` (ctest `reactor_limits`): [T] targets on FreeBSD, macOS and Linux limit
types with values; [R] the real raise (moved from reactor_test); [M] SIZE_MAX to the hard limit;
[C] the macOS cap at maxfilesperproc + 1; [S] a refused setrlimit returns the soft 1024 in force.

reactor:01db032. `reactor::tls::seconds_to_expiry` returns `std::optional<long>`: nullopt for no
certificate or an unreadable notAfter, 0 or negative for expiring now or expired. Signature change
(lead's ruling); no caller outside reactor. Xapiand's own `tls_seconds_to_expiry` is filed
separately to move onto it.

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
- The unreadable case is built by loading a real certificate and replacing its notAfter bytes with
  "not a time" (`ASN1_STRING_set`), then `SSL_CTX_use_certificate`: no file, no signature check.
- docs/API.md:103 and AGENTS.md:81 say "the time left"; still true, left alone (outside the file set).
