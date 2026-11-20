# freebsd-exe-path review: correctness

Commit: 53b316116
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding.

## Evidence I relied on

- Job 261119-151950-269667 (macOS targeted build and run of the new test) and suite job 261119-151953-11ac9b: both passed per the journal. They show the test links (the `opts` rename resolves the duplicate symbol) and passes on macOS. They say nothing about the FreeBSD or Linux branches; those come from CI.

## What I checked myself

Read only, no probe jobs: the diff `origin/master...53b316116` in src and tests, the CMake test glob (CMakeLists.txt:1909-1937), `tests/test_stubs.cc`, `src/opts.h`, `src/xapiand_fsyncher.h`.

1. sysctl branch (src/main.cc:956-962).
   - The mib `{CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1}` is the documented self query (pid -1); it is a 4-int array passed with count 4.
   - FreeBSD copies the path out with its terminating NUL, so `len` includes the NUL. `len > 1` therefore means "a non-empty path", and a kernel that cannot resolve the vnode path (returns "" with len 1) falls through. If `len` excluded the NUL the code would still be right, because `buffer[len] = '\0'` lands in bounds.
   - Bounds: `len` goes in as `PATH_MAX` and comes back at most `PATH_MAX`, so `buffer[len]` is at most `buffer[PATH_MAX]`, inside `char[PATH_MAX + 1]`. A longer path makes the kernel return ENOMEM, and the code falls back. Returning `buffer` (a `char*`) stops at the first NUL.
   - Fallback: any failure or an empty answer reaches the existing `realpath(argv0)` then raw `argv0` then empty. Linux and macOS branches are untouched.
   - Includes: `sys/types.h` then `sys/sysctl.h`, guarded by `__FreeBSD__`, so other platforms are unaffected.
2. Test (tests/test_executable_path.cc).
   - It runs on every platform through the glob and is named `xapiand_test_executable_path`, so the `name` suffix check matches the target name. It calls `get_executable_path(nullptr)`, so only the kernel (macOS `_NSGetExecutablePath`, Linux `/proc/self/exe`, FreeBSD sysctl) can answer; without the fix FreeBSD returns "" and `exe.size() > 1` fails. The check is therefore a real A1 witness on FreeBSD. It also asserts the name suffix, `access(X_OK)` and that a bogus argv0 is ignored.
   - The `opts` rename: `opts_t main_opts;` is defined in this TU and `test_stubs.cc` defines `opts`, so there is no duplicate. `get_executable_path` does not use `opts`, so what is tested is unchanged. The `#define main` rename is the same one the coverage-core-main test uses.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | nit | The `#define opts main_opts` is active while main.cc's own includes are processed, so any header it pulls in that uses `opts` inline is compiled in this TU against `main_opts`: `inline auto& fsyncher()` (src/xapiand_fsyncher.h:73) and the macros in src/xapiand_io_random_errors.h:45-55. That is an ODR divergence from every other TU that reads `opts`. Harmless here (both objects are default-constructed and the test never starts a fsyncher), but latent if the test ever grows. | src/xapiand_fsyncher.h:72-73; tests/test_executable_path.cc:28-31 | Include main.cc's own headers before the `#define`s, as the coverage-core-main test does, so the rename touches only main.cc's text. |
| F2 | no | gap | The FreeBSD sysctl branch and the Linux build of the new test have not run; only macOS has. Not a defect in the code as read. | journal Evidence table | FreeBSD and Linux CI at 9f8b95f74 or later must be green before landing (A1, A3). |
| F3 | no | nit | A2: the workaround (e0ad77c2c) is not on this branch, so it is correctly noted and not edited; its quest should call `get_executable_path(nullptr)` and drop its FreeBSD sysctl block once this lands. | journal Rulings; `git show e0ad77c2c --stat` | Carry the follow-up to the coverage-core-main quest. |

## Not run

- No probe jobs, no FreeBSD or Linux build or run, and no check that `main_opts` is not referenced by a header at link time beyond reading the includes (the macOS job linking is the evidence).
