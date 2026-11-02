# Security review: Phase 7c X4 (Lua in a yielding coroutine, D10)

Branch `phase7c/x4-lua` (`Xapiand:22d6ff437` to `Xapiand:ef7980ca9`), worktree `~/code/Kronuz/Xapiand-wt/x4`. Reviewed against the ledger
`docs/status/phase7c-x4.md`, design `docs/design/phase7c.md` D10 and T10, and `docs/reviews/phase7c-design.md` R1 and C11 to C13.
`docs/reviews/calibration.md` does not exist in this tree, so severities below follow the older security reviews in `docs/reviews/`.
This file covers two reviewer sessions: the first (stopped partway) established the sandbox surface and found X4-S1; the second measured the
remaining bounds. Did not open `src/sortable_serialise.{cc,h}`. No `/tmp`; probes, harnesses and outputs are in
`.scratch/review-x4/` (kept, listed at the end). Servers were Release builds on ports 18981 to 19081 and every one was killed with SIGKILL.
Per the brief, this file describes each check by category and result; the probe scripts hold the exact inputs.

## Verdict: REJECT as it stands (four blockers, all with small fixes)

The sandbox surface is sound (nothing reachable beyond the string, table, math and utf8 libraries, locked metatables, per-call libraries). The
bounds are not: one cheap input class defeats the instruction cap and wedges a reactor for good (ten such requests make the whole node
unresponsive), the "no Lua instructions, so bounded by the byte cap" argument in D10 is false for time, the converted result is not bounded
by anything the cap covers, and two clients can swap each other's cached scripts. The harness `harness/lua_bounds.py` passes because it only
tries the bound-swallowing shapes the developer thought of.

## Threat model

Attacker: a network client who can write a document (or a schema) that carries a script, or who can name a foreign script in one. That is
anyone who can use the HTTP write API; with auth off, anyone who reaches the port. Goal: stall or kill the node, exhaust its memory or its log
disk, or change what another client's writes do. The trusted-cluster rule (perimeter plus honest faults) does not apply to this surface: the
sandbox exists to hold scripts written by clients, and D10 states its bounds as the defence.

## Already established (first session, not repeated)

No reachable `io`, `os`, `debug`, `package`, `require`, `load`, `loadfile`, `dofile`, `collectgarbage` or coroutine library; the string
metatable and the environment metatables are locked; `__gc` is refused; per-call isolation of `string`, `table`, `math` and `utf8` holds.
Probe: `p1_escape.py`. Minor, unchanged: `tostring(function)` shows a heap address; `math.random`'s state is shared across calls of one script
(a covert channel between clients that share a foreign script).

## Blockers

### X4-S1 (high): a to-be-closed variable lets a script outlive the instruction cap and monopolize a reactor for good

**Attacker:** any client that can write a script. **Gain:** one request pins one reactor thread at 100% CPU forever; with a request per reactor
(10 on this 16-CPU host) every read and write on the node times out, and a graceful stop does not complete.

Mechanism: the hook ends a call by raising a sentinel table, and the base `pcall`/`xpcall` recognise a bound only by the identity of the error
value (`processor.cc` `fatal(e)`: `e == KILL`, "not enough memory", the matcher's message). Lua 5.4 runs the `__close` handler of a live
to-be-closed variable while an error unwinds, and an error raised by that handler replaces the in-flight error. The sentinel is therefore
replaced by an ordinary string, `fatal` does not recognise it, and the script's own `pcall` catches it. The hook raises again 10,000
instructions later (the reason stays set), and the script catches that one too. Because the hook raises instead of yielding once a bound is
set, the call never returns to the reactor.

Measured (Release binary, probe `p14_close_swallow.py`, first session `p2_close_swallow.py`, `p2b_close_swallow_dos.py`):

| Check | Result |
|---|---|
| One such call, fresh-connection `GET /` for 12 s | node CPU 100%; the requests that landed on that reactor timed out (2 of 20); the call never ended |
| Graceful stop (SIGINT) with that call running | node still running after 20 s ("HTTP is busy" repeating in the log) |
| N such requests in flight, 20 fresh GETs each time | answered 20, 16, 14, 12, 10, 8, 6, 4, 2 of 20 for N=1..9; **0 of 20 at N=10**; a plain write then timed out |
| The same loop through `xpcall` | also runs past the cap, GET timed out on the landing reactor |
| Control (`p2c_control.py`): a `pcall` loop without a closing variable | ends at the cap with 400 in about 0.56 s, as the ledger says |

M8 in the ledger ("`pcall` cannot swallow a bound") is a mutant of the wrapper, not of this path, which is why it was not found.
Not a wedge but worth knowing: a variant that closes a generic-for closing value ran 14 requests at 850% CPU with all fresh GETs answered
(`p14b_genfor.py`), so it yields; only the form where the closing variable is live across the endless loop wedges.

Fix (either alone closes the probe; do both):
1. Give the base a host function that reports whether the call already crossed a bound (`call->reason != nullptr`) and have `after()` in
   `pcall1`/`xpcall1` rethrow whenever it is true, whatever the error value is. Errors can only be stopped at those two functions (no
   coroutine, no `load`, no C-level protected call is exposed), so a flag there is robust against any other way of replacing the value.
2. Refuse a `__close` metamethod in `setmetatable1` the way `__gc` is refused, or, where the call is yieldable and a bound is set, make the
   hook yield and have `operator()` end the call from the host side (a yield cannot be caught by a script's `pcall`).
Tests: a unit test and a `lua_bounds.py` case for the closing-variable shape in both `pcall` and `xpcall`; a mutant that drops the flag check.

### X4-S2 (high): library and VM work that the hook cannot see is unbounded in time (D10's "bounded by that cap's sizes" is false)

**Attacker:** any client that can write a script. **Gain:** a reactor thread (and its connections) stalled for seconds to days per request,
without crossing the byte, instruction or step caps; a stalled reactor also blocks shutdown. All rows below were run on the Release node while
other agents' builds ran on the host, so absolute times are noisy by perhaps 2x; the scaling is what matters.

Item 1 of the brief (one non-allocating call over the largest string the cap permits). The largest single string is 31.98 MiB (21.29 MiB each
for two live strings) (`p3_maxstring.py`). Per-call wall time (`p4c_rep.py`, `p4_cstall.py`, `p4b` rows in `out/`):

| One call | Result |
|---|---|
| plain `find` of an absent 1-byte needle, 31 MiB | 2.9 ms; the no-magic-pattern form 3.2 ms |
| `utf8.len`, 31 MiB | 87 to 89 ms |
| string comparison, two equal 15 MiB strings | 5.5 ms |
| `upper` / `reverse` / `rep` on 20 to 31 MiB | 89 / 43 / 9 ms |
| native `table.sort` (no comparator), 1M numbers | 0.5 to 0.8 s (2M: about 1.0 s) |
| `table.concat`, 2M short strings | 92 ms |
| front `insert` or `remove` on a 2M-element array | about 30 ms each |
| one `gsub` that completes over 8 / 31 MiB | 216 / 482 ms |
| a quadratic pattern run to the step budget's end, 1 / 4 / 16 / 31 MiB | 24 / 470 / 1,085 / 1,935 ms, then 400 |

These alone are bounded by the byte cap, but not acceptable for a reactor (T4 asks for a 1 ms p99), and the bound does not hold under repetition:
the instruction hook cannot fire inside a C call, so a loop of such calls runs about 10,000 / (instructions per iteration) iterations between
two yields. 1,400 iterations of the cheapest one (plain find, 31 MiB) ran 3.5 s uninterrupted (`p16_slice.py`), and a fresh GET waited 16.3 s
on that reactor (`p7_loops.py`). The same loop shape over `utf8.len`, native sort, front insert/remove and a 31 MiB `rep` left GETs on the
landing reactor timing out at 20 s. So the answer to brief item 1 is: **bounded per call by the memory cap, not acceptable, and it needs more
than a per-call length guard**, because the multiplier is the loop.

Unbounded in a single call (the stronger finding):
1. **Substring search is quadratic.** `string.find` with a plain flag, or with a pattern that has no magic characters, uses `lmemfind`
   (unpatched), which is O(haystack x needle) in the worst case and is not covered by the matcher's step budget. Measured, haystack/needle:
   64K/32K 71 ms, 128K/64K 258 ms, 256K/128K 2.2 s, 512K/256K 6.8 s, **1 MiB/512 KiB 15.9 s**, always answering 204. Extrapolated at the
   measured rate (16.5 GB/s of compares) to the largest pair the cap allows (about 21 MiB / 10 MiB): **about 1.8 hours in one call**, no hook,
   no cancel, and no shutdown. (`p4_cstall.py quad`.)
2. **The matcher's step budget counts `match` entries, not work.** The patch increments `steps` once per `match` call, but a run of literal
   characters and a bracket-class scan are walked inside one `match` entry (the `goto init` loop and `matchbracketclass`). A pattern with a
   long literal run before a magic character, or a long bracket class, costs O(subject x pattern) while using about one step per start
   position, so it never reaches 8 x subject length. Measured (`p13_matcher_quad.py`, answering 204, budget never tripped), subject / pattern:
   16K/8K 0.55 s (literal run) and 0.42 s (class), 32K/16K 2.4 / 1.2 s, 64K/32K 8.9 / 7.1 s, **128K/64K 49 / 31 s**; four-fold per doubling.
   Extrapolated to the cap's sizes: weeks. The drift test passes because the patch is what the ledger says; the budget is the wrong unit.
3. **VM operations whose cost grows with stack depth are one instruction to the hook.** A call chain that passes a growing argument list ran
   more than 120 s without reaching the instruction cap; the reactor's worst fresh-GET stall grew 1.6, 2.6, 4.2, 4.5 s over 40 s while the
   call kept going (`p18_vararg.py`; `p8_recursion.py` saw the same call time out at 120 s).
4. **Garbage collection is outside the hook.** A weak-keyed table holding a long chain makes the collector's convergence pass super-linear:
   script wall time 1.5 / 7.2 / 18 / 61 s for 20K / 40K / 80K / 160K entries (loaded host), and with 300K entries (well under the byte cap)
   **one 56 s stall of the reactor** with 120 of 1,571 concurrent GETs over 1 s (`p15_gc_compile.py`, `p19_gc_stall.py`). A control without the
   weak table took about 0.5 s.

Fix direction: charge work, not entries.
1. Add the work to the call's counter. In `match`'s `default` run and `matchbracketclass`, and in `lmemfind`, add the bytes examined to
   `ms->steps` (the budget is then max(10^5, 8 x subject) units of real work); or replace `lmemfind` with a linear search. This closes items 1 and 2.
2. For linear library calls, wrap the base's copies of `string`, `table`, `utf8` (already per-call proxies) so each call charges
   size / k instructions and yields at the next hook, and clamp the sizes of the few calls that cannot be sliced (native `sort` without a
   comparator can use the Lua merge sort that already exists; `utf8.len`, `concat`, front insert/remove need a length cap such as 1 to 4 MiB).
   That removes the loop multiplier; a per-call guard alone would leave 1,400 x (a few ms) per slice.
3. Bound the collector's pause: run it in a mode with a small step, or refuse weak tables (`__mode`) in `setmetatable1` the way `__gc` is
   refused. Bound stack-depth-proportional VM work with Lua's own limit (`LUAI_MAXSTACK` is 1M slots; lower it, or count a vararg call as
   its argument count in the hook).
Tests: T10 gains one case per category (search, matcher unit-of-work, vararg chain, weak table), each asserting a 400 or a bounded stall on a
live node, and T4 is rerun with four looping library-call scripts rather than four pure-Lua loops.

### X4-S3 (high): the converted result is built outside the byte cap and is not bounded by anything

**Attacker:** any client that can write a script. **Gain:** a 100-byte request makes the node allocate gigabytes; a few requests kill it
(out-of-memory), and the conversion runs on the reactor without a yield point.

Brief item 2. `lua_to_msgpack` walks the returned `_doc` after the call, with the cap switched off (`heap->metered = false`), and limits only
node count (`max_bytes / 16`, 4,194,304 nodes) and depth (200). It copies string bytes once per reference, and a table that shares its parts is
walked once per reference. Peak host RSS, sampled every 20 ms with a 6 GB kill (`p5_conv.py`; every Lua heap here stayed under 64 MiB):

| Result built | Status / time | Peak RSS |
|---|---|---|
| one 1 MiB string referenced 16 / 64 / 256 times | 204, 0.9 / 3.0 / 11.5 s | 272 / 784 / 2,444 MB |
| one 4 MiB string referenced 64 / 256 times | 204, 10.7 / 43.5 s | 2,018 / 5,162 MB |
| one 8 MiB string referenced 256 times | killed at the 6 GB watchdog after 55 s | over 6,000 MB |
| one small object referenced 100,000 / 500,000 times | 204, 3.2 / 14.8 s | 880 / 4,302 MB |
| the same small object referenced 2,000,000 times | killed at the watchdog after 9.7 s | over 6,000 MB |
| 1,000,000 distinct short strings (no sharing, about 50 MB of Lua heap) | 204, 24 s | 5,989 MB |

Extrapolated at the measured per-reference cost, one 31 MiB string referenced a couple of million times is terabytes, and the 4.2M-node cap
allows tens of gigabytes. The control (`p17_direct_doc.py`) shows the document pipeline costs about 500 bytes of host memory per node however a
document arrives: a plain 10.4 MiB body of 1M strings also peaks at 5.4 GB, and a 256 MiB body is refused by the HTTP layer. So the engine's
per-node cost is pre-existing; what is new, and what the "bounded" claim in the ledger misses, is that the script path reaches that cost from
a tiny request, past the body cap that protects the direct path. The cycle and depth checks do work (cycle, 2^60-path DAG and 100,000-deep
nesting each end in 400 in 0.04 to 5 s with RSS under 250 MB, `p5_conv.py`, `p8_recursion.py`).

Fix: charge the conversion to a budget equal to the body cap that guards the direct path: count string bytes as well as nodes
(`walk.bytes_left`), take the smaller of that cap and the 4.2M nodes, and choose the node limit from the per-node host cost rather than the Lua
heap's 16 bytes per slot. Run the walk in slices or under the same budget so it cannot hold a reactor for 10 to 50 s.
Test: a result with a shared large string and a result with shared small objects each get a 400 and RSS stays under a stated bound.

### X4-S4 (medium): a local script's name and a foreign script's cache key share one namespace, so clients can swap each other's scripts

**Attacker:** a client that can write a document carrying a local script with a name of its choice. **Gain:** the script that runs for another
client's write that references a shared foreign script, and so that client's stored documents.

Brief item 7. Local scripts are cached under the script's name (`_name`, which the client chooses); a foreign script under the reference
string it was written with (`compile`: `key = script.get_endpoint()`), in the same per-thread LRU, and a foreign hit is returned without
comparing anything (`cached(key, {})`). Probe `p10_collision.py`, two directions, each against a one-reactor node and a default node:

| Case | Result |
|---|---|
| foreign script already cached, then a local script is saved under the same name | victim's later writes ran the attacker's script (8 of 8 on one reactor; 6 of 8 on the default node, the rest on reactors that had not cached it) |
| attacker's local script saved first, then the victim's foreign reference | victim's writes ran the attacker's script (8 of 8 / 6 of 8) |
| the shared foreign script is rewritten after being cached | victims keep getting the old (or the attacker's) script; the cache never revalidates |

Fix: namespace the key (for example a prefix that a client-chosen name cannot produce, or a separate LRU per kind), and decide whether a foreign
entry is revalidated (document version or a short age). `remember`'s replace-on-hash-change is correct for local scripts under one name, but it
is what lets a client evict the foreign entry.
Test: the two orders above, asserting the foreign script still runs for the victim.

## Non-blocking findings

- **X4-N1 (medium): `print` and error messages are not capped or rate-limited.** (Brief item 4, `p9_log.py`.) `print` is the C library's, so it
  writes to the process stdout, not through the logger: 1,000,000 one-byte prints took 6.5 s and 1.9 MiB; one request printing an 8 MiB string
  twenty times wrote 160 MiB in 0.22 s. A script error logs its whole message with `L_ERR`: log lines of 8,388,687 and 32,505,935 bytes were
  written by two requests, and 200 requests with 1 KiB messages wrote 200 lines of 1,103 bytes. Compile messages ("compiled and ready") are
  not shown at the default verbosity (2,000 requests with distinct names and a 1 MiB name logged nothing). An attacker fills the log disk or
  stalls on a slow log reader. Fix: replace `print` with a function that logs through `L_*` with a per-call byte budget, and truncate the
  script error message at a few KiB; a per-call count of log writes is enough.
- **X4-N2 (medium): the cached state is bounded, but the bound is large.** (Brief item 3, `p6_cache.py`, `p6c_leak.py`.) The LRU holds 10
  compiled scripts per thread, each with its own Lua state, and a state keeps its heap garbage after a call because nothing forces a
  collection. After 600 calls of one script RSS was 761 MB; 2,000 calls of a trivial script added 13 MB; 300 calls that crossed the cap added
  92 MB; 440 distinct scripts that each built 24 to 32 MiB left RSS at 938 MB and flat after 10 s idle. No growth that tracks the call count, so
  no leak, but the ceiling is threads x 10 x 64 MiB (several GB on a 16-CPU host). Fix: run a full collection when a call ends (or when the
  state's live bytes exceed a fraction of the cap at call end), and size `--scripts-cache-size` against that ceiling in the docs.
- **X4-N3 (low): script errors other than a bound return 2xx and leave the document unchanged** (unchanged behaviour, stated in the ledger). A
  validation script that raises is silently skipped: every recursion/overflow, runtime-error and `error()` case in `p8_recursion.py` and
  `p9_log.py` answered 204 and stored the document. Worth a 4xx for scripts, since clients use scripts as guards.
- **X4-N4 (low): recursion (brief item 5, `p8_recursion.py`).** Lua stack overflow, C stack overflow (pcall nesting, `gsub` callbacks,
  `__index`/`__tostring`/`__concat`/`__lt` metamethods, comparator recursion), over-deep syntax (nested parens, tables, function bodies, long
  concatenation chains) and a 100,000-deep result all end with a logged script error or a 400; no crash, node alive after the whole set. The
  one non-terminating shape is the vararg chain in X4-S2.
- **X4-N5 (low): script compile is synchronous, unmetered, and bounded by the HTTP body cap.** A 4 MiB script body compiled in 0.3 s and held
  56 MB of RSS (it stays cached); bodies of 16 and 32 MiB were refused by the HTTP layer before reaching Lua. 300 distinct trivial scripts cost
  the same per request as one script (48 ms, dominated by the commit) (`p15_gc_compile.py`).
- **X4-N6 (low): the drift test's reach.** (Brief item 6, `p12_drift.sh`.) The patch reads correctly: `steps` and `budget` are set in
  `prepstate` (once per API call; `reprepstate` leaves them), counted at `match` entry, and the error is the one the base rethrows; the
  one-per-call budget also spans a whole `gmatch` iterator (budget of the subject for the entire iteration, which could 400 a legitimate heavy
  `gmatch`; the ledger says 1,000,000-byte inputs pass). Drift test on a scratch copy of the compiled tree: unmodified passes; a trailing
  comment appended to `lstrlib.c` fails (exit 1); a one-digit change of the budget constant fails; a benign edit of `lvm.c` is not noticed (only
  `lstrlib.c` and the tarball hash are checked; the tarball hash is enforced by the fetch, but a `FETCHCONTENT_SOURCE_DIR_LUA` override skips
  it) and the test only runs with `BUILD_TESTS`. Consider hashing the whole `src/` tree of the compiled source.
- **X4-N7 (low): `lua_call` and the allocation of a call's tables run unprotected and unmetered** (`operator()`, `push_msgpack`), so a real
  out-of-memory there reaches Lua's panic handler (abort) rather than a 400. Reading only; not provoked. Run those steps under `lua_pcall` or
  set a panic function that throws.
- **X4-N8 (low): carried from the first session, unchanged.** Heap address in `tostring(function)`; `math.random` state shared across calls
  of a script; the byte cap is shared by interleaved calls on one script (a hog can fail its sibling, stated in the ledger).
- **X4-N9 (info): missing foreign scripts (brief item 8, `p11_missing.py`).** 24 reference shapes (missing document, missing database, empty or
  malformed references, a document without a script field, a non-object script, a nested or self-referencing foreign script, a syntax error, a
  looping script, 100,000-character references, traversal-looking and percent-encoded paths) each answered 400 in at most 0.54 s (the looping
  one, at the instruction cap); 300 repeats of a missing reference took 0.27 s in total; a plain write afterwards answered 204 and nothing
  blocked. The response echoes the reference back, up to 100,000 characters (only to the sender).

## Brief items, one line each

| # | Check | Result |
|---|---|---|
| 1 | one non-allocating call at the cap's largest string | 3 to 90 ms per call (fine), but loops multiply it to seconds and quadratic search/matching is unbounded: X4-S2 |
| 2 | host memory converting the result | not covered by the cap, hundreds of times the Lua heap: X4-S3 |
| 3 | memory across cached scripts | bounded by threads x 10 x 64 MiB, no per-call growth: X4-N2 |
| 4 | log volume | not capped or rate-limited: X4-N1 |
| 5 | recursion | no crash; errors only: X4-N4 (one non-terminating shape in X4-S2) |
| 6 | patch and drift test | patch reads correct, drift test bites on `lstrlib.c`; the budget's unit is wrong: X4-S2, X4-N6 |
| 7 | name collision with the foreign key | collides in both orders: X4-S4 |
| 8 | missing foreign script | 4xx, nothing blocks: X4-N9 |

## Conditions to land

1. X4-S1 to X4-S4 fixed, each with the test named above, and the first session's closing-variable probe (`p14_close_swallow.py`) rerun:
   the call ends with 400 within a second and ten of them leave the node serving.
2. `harness/lua_bounds.py` and T10 extended with one case per category above; T4 rerun with library-call loops.
3. X4-N1 and X4-N2 either fixed or ticketed before a release exposed to untrusted clients; the others at the developer's discretion.

## Not run

TSan and ASan builds of the fixes (none exist yet); a Linux or FreeBSD node (probes ran on macOS arm64 only, so `memchr`/`memcmp` rates, the
allocator's retention and the stall times differ elsewhere); the cluster paths (a foreign script read across nodes, replicated writes); TLS;
`harness/lua_bounds.py` itself (reproduced by hand); a fix-side re-measurement of any finding; the vararg and weak-table stalls on an idle
host (the host ran other agents' builds and suites throughout, a 4-job `quiet light` hold at a time); the 128K to 512K and larger matcher cases
and the 16 s-and-longer search cases were not run to their end (scaling extrapolated from the measured points, labelled above); the utf8 and
native-sort loops of `p16_slice.py` (stopped, per-call times used instead).

## Probes and outputs (all in `.scratch/review-x4/`, kept)

First session: `lib.py`, `p1_escape.py`, `p2_close_swallow.py`, `p2b_close_swallow_dos.py`, `p2c_control.py`, `run-*` dirs.
Second session: `s_lib.py` (shared helpers; nodes are SIGKILLed at exit), `p3_maxstring.py`, `p4_cstall.py`, `p4c_rep.py`, `p5_conv.py`,
`p6_cache.py`, `p6c_leak.py`, `p7_loops.py`, `p8_recursion.py`, `p9_log.py`, `p10_collision.py`, `p11_missing.py`, `p12_drift.sh`,
`p13_matcher_quad.py`, `p14_close_swallow.py`, `p14b_genfor.py`, `p15_gc_compile.py`, `p16_slice.py`, `p17_direct_doc.py`, `p18_vararg.py`,
`p19_gc_stall.py`, drivers `run_all_a.sh`, `run_all_b.sh`, `run_all_c.sh`, outputs in `out/` (`p3.txt` ... `p19b.txt`).
Run each as `quiet light NAME MINUTES WHAT -- python3 pN_*.py PORT` (ports 18981 and up).


## Re-check 1

Scope: X4-S1 to S4, N1 and N3, the developer's two residuals, and what the limits patch changed for normal scripts. Not a new review. Branch head
`Xapiand:2752325bb` (`a62c1277a`, `9729de23e`, `4700151bf`, `ee597976d`, `2752325bb`), tree clean. Fresh Release build in
`.scratch/review-x4-recheck/build-rel` (`cfg.sh`; dependency sources reused from the developer's `deps-rel`, Lua fetched fresh so the patch
applied at configure). `ctest -R lua`: `test_lua` and `test_lua_patch_drift` 2 of 2. Did not open `src/sortable_serialise.{cc,h}`. Every server was
started by a probe and SIGKILLed at exit; none left running; ports 19300 to 19900 only. Host load average 5 to 8 on 16 CPUs throughout, so times
are noisy by tens of percent; the scaling and the orders of magnitude are what matter. Checks are described by category and result.

### Rulings

| Item | Ruling | Why |
|---|---|---|
| X4-S1 closing variable outlives the cap | **CLOSED** | `p14`: the call ends (400 at the instruction cap), SIGINT exits within 20 s, 1 to 39 in flight leave 20 of 20 fresh GETs answered (max 4 ms), a plain write answers 204, `p14b` (14 generic-for swallowers) 20 of 20 at 954% CPU, `p2`, `p2b`, `p2c` end in 400. The language now refuses `<close>`, a generic-for closing value and a `__close` in `setmetatable`; the flag and the per-instruction raise are depth. See residual (b). |
| X4-S2 work the hook cannot see | **PARTIAL** | The unbounded classes are closed: quadratic plain find 400 in 0.3 s (was 1.8 h extrapolated), matcher literal-run and class shapes 400 (`p13`, 512K/256K 190 ms), vararg chain ends (`p18` GET max 22 ms), weak-table GC stall gone (`p19` max 6 ms over 108 GETs; the table is never weak), and the loop multiplier is gone (`p7` GET max 1.3 to 14 ms for plain find, `utf8.len`, sort, front insert, 31 MiB `rep`). What remains is the per-call stall of a call the hook cannot interrupt: up to the work cap, 0.55 to 0.85 s, for a matcher call, attacker-chosen. New finding X4-R1 below. |
| X4-S3 converted result | **CLOSED** (memory); time is X4-R2 | `p5`: every shape that cost 2.4 to 6 GB now answers 413 (or 400 at the memory cap) in 0.1 to 0.5 s with peak RSS 15 to 275 MB (was over 6,000 MB). References are counted per reference. 413 is the status the tests assert. |
| X4-S4 cache key collision | **CLOSED** | `p10`, both directions, one reactor and default node: the victim keeps `legit` before and after the attacker's same-named local script and when the attacker seeds first; a rewritten foreign script is seen (`rewritten`) after the 1 s trust (the default node's first request inside that second still ran the old one, as stated). Keys are `L`+name and `F`+reference. |
| X4-N1 print and error volume | **CLOSED** | Default verbosity: log +0.00 MiB for every `p9` case. The default hides `L_INFO`, so also run at `--verbosity 6` (`rp_print.py`): 1,000,000 one-byte prints cost 145 KiB (the 4 KiB budget counts payload, not the line overhead of about 35 bytes, so the worst case is about 145 KiB per request), 20 x 8 MiB prints 1.8 KiB, a 31 MiB error message 1.7 KiB (cut at 1 KiB), 200 requests with 1 KiB messages 328 KiB. Was 160 MiB and 32 MiB lines. The unit test does not bite (R3). |
| X4-N3 script errors answered 2xx | **CLOSED** | Script errors answer 400 with the message (cut at 1 KiB): `p8`, `p9`, `p14`. A normal script that raised before now fails its write; documented in the changelog and `scripting.md`. |

N6 (drift test reach) is also closed: the test now compares every file of the built tree and the file list. On a scratch copy of the built tree:
unmodified exit 0; a comment appended to the unpatched `ltable.c` exit 1; an extra file in `src/` exit 1; a changed `LUAI_MAXSTACK` in `luaconf.h` exit 1.
`harness/lua_bounds.py` on the fresh binary: PASS (30 cases at their statuses, 10 swallowers answer 400 with 20 of 20 fresh GETs, graceful stop, foreign
script in both orders and the rewrite, light GET p50/p99/max 0.49/0.73/1.28 ms alone, 0.48/0.72/1.37 ms beside 4 long scripts, 0.52/1.88/2.34 ms beside 4
library-call loops).

### Fix reverted, test fails? (mutants on the built objects; scratch copies only, the tree is untouched: `mut.py`, `mut/`, `out/mut*.txt`)

| Mutant | Result |
|---|---|
| S1: close ban dropped (`lua_xcloseok` true, `setmetatable` accepts `__close`); same plus flag and per-instruction raise dropped (the original bug) | KILLED, `test_a_script_cannot_declare_a_to_be_closed_variable` (the process aborts there) |
| S1: flag dropped from `fatal()`; per-instruction raise dropped (flag kept); both dropped | all three SURVIVE the unit tests (see residual (b): the test shapes cannot tell them apart) |
| S2: work cap never condemns | KILLED (hang, the test watchdog) |
| S2: `lmemfind` uncharged; matcher `goto init` run uncounted; vararg/results move uncharged; weak tables weak again; `utf8.len` uncharged | KILLED (`:607`, `:626`, loop cases, `:659`, hang) |
| S3: string bytes unbounded; container weights zero | KILLED (`:785`, `:796`) |
| S4: local and foreign share a namespace | KILLED (`:818`, `:826`); 1 s revalidation made 1 h: SURVIVES the unit tests, covered by the harness and `p10` |
| N3: script errors swallowed again | KILLED (`:185`) |
| N1: print budget removed | SURVIVES (R3) |

### Residual (b): the surviving flag mutant

Does any path rely on the condemned flag alone? **No.** With the language ban in force no script can replace an in-flight error, so `fatal()` never
sees a replaced value. With close handlers permitted (the unit test's `allow_close`), a 4-way matrix (`probe_close.cc`; shapes: `pcall`, `xpcall`,
generic-for, nested `pcall` inside the handler, all with a handler that raises a non-string) gives: shipped ends in 0.01 s; flag dropped ends in 0.01 s;
per-instruction raise dropped (flag kept) ends in 0.01 s; **both dropped does not end** (25 s timeout). So the flag and the per-instruction raise are each
sufficient and neither is solely relied on. One more thing found by tracing (`mut/trace_both.cc`): the shipped tests' handlers raise a *string*, and a string
reaches `find(e, "(step budget)", ...)` inside `fatal()`; `find` is a charged library call and raises the sentinel once the call is condemned, so the
tests end for a reason unrelated to the flag or the hook. With a table-valued error that incidental layer is gone. Consequence for the tests, not the code:
the "both layers dropped" mutant survives because of that incidental layer; change the handlers in `test_a_close_handler_cannot_outlive_a_bound` to
`error({})` and that mutant dies (R3). Accept the flag mutant surviving as defence in depth.

### Residual (a): a call the hook cannot interrupt

Measured on the fresh Release build, one HTTP server (so the GET shares the script's reactor), worst fresh GET while one request runs one call, minus the
same script without the call (`ra_stall.py`, `out/ra.txt`; `p4*`, `p7`):

| One call | Reactor held |
|---|---|
| matcher call that runs to the work cap and answers 400 (`find('.-b')`, `a*a*a*b`, `.-.-.-b`, `[ab]*c`, on 8 or 31 MiB) | **0.5 to 0.85 s** (worst GET 700 to 870 ms; the cap is 5 x 10^8 units at about 1.1 to 1.7 ns a unit) |
| `gsub('a','b')` that completes, 8 MiB / 31 MiB | 0.1 to 0.4 s / 0.5 to 0.8 s (the 31 MiB one ends at the memory cap) |
| linear library calls: `utf8.len`, `format('%s')`, `table.concat` 2M, front `insert` 2M, `move` 2M, `upper`, `rep` 31 MiB | 0.03 to 0.22 s each |
| the conversion of a result at its caps (262,000 integers; `sample` of the node: `lua_to_msgpack` 689 of about 3,960 sampled ms, about 2.7 us a value) | **about 0.7 s** |
| the whole PUT of that result | 5.0 s, worst GET 4.8 s; the same document sent as a plain body: 3.0 s, worst GET 2.6 s |

The matcher number is the one that matters, because the attacker chooses it and every request costs about 200 bytes. Sustained (`rc_flood.py`, default
node, 12 s, K clients each re-sending the request as soon as the 400 returns): K=4 hostile requests: light GET p50 1.2 ms, **p99 829 ms**; K=16 (about one
per reactor): **p50 468 ms, p99 1,871 ms**, 13 GETs answered in 10 s; the control with yielding Lua loops, K=16: p50 1.7 ms, p99 51 ms. The node is not
wedged and recovers when the attack stops, but T4's rule (p99 at or under 1 ms) does not hold under hostile scripts and one client with a handful of
connections can hold every reactor.

**Ruling: not a blocker for landing X4; a blocker for any release that runs scripts from untrusted writers.** Reasons: it is bounded and ends in a 400, the
node stays up and stops gracefully, nothing is stored, and the unbounded classes the first review blocked on are closed. It is the same posture as N1 and
N2 in the first review's conditions. The fix is small and belongs to this patch: **a per-call cap in work units, not in bytes.** A byte cap per call
(say 1 MiB) is not enough: the matcher's cost is steps, not bytes, and a quadratic pattern over a 1 MiB subject runs 64 x 10^6 steps (130 to 330 ms in
`p4b`) before its budget ends it, so a byte cap leaves a 0.1 to 0.3 s stall and a flood with the same shape. Instead, add `max_call_work` (about 16 x 10^6
units, 15 to 25 ms at the measured rate) and (i) make `lua_xcharge` refuse a single charge above it, which stops the linear calls before they run (they
charge first), and (ii) set the matcher's `ms->total` to `min(max(10^6, 64 x subject), max_call_work / 4)` so one API call cannot run past it. It reuses the
meter, the sentinel and the tests' shapes; normal scripts (strings under about 2 MiB, tables under about 1M elements per call) are unaffected, and a loop
of such calls still yields between calls. Then state the bound honestly in D10 and T10: single non-yieldable call at most about 20 ms, so the 1 ms p99
holds for benign scripts and is bounded at about 20 ms for hostile ones. Do not claim "no reactor stall" before that.

The conversion: **accept with a stated bound, do not make it a chore on its own.** Reasons: at the node cap it holds the reactor about 0.7 s, the same
magnitude as R1's cap; it sits in front of the write's own indexing, which holds the same reactor 2.5 to 4.8 s for the same document sent directly (a
pre-existing property of the write path, `p17`: a 10 MiB body of 1M strings peaks at 5.8 GB and takes 10 s), so a conversion-only chore would shave
under a fifth of a stall the write path still has; and chunking belongs with that path's own work. Record in D10: conversion at most 0.7 s for a
result at its caps, charged by the same weights. If R1's per-call cap lands, make the conversion's weights obey it too (262,144 units become a
single-digit-millisecond slice only if the node cap falls to a few thousand values, which breaks normal documents, so do not).

### New findings

- **X4-R1 (medium, blocks exposure to untrusted writers, not the landing): a single non-yieldable call holds a reactor up to the work cap.** Attacker: any
  client that can write a script. Gain: with K connections at about one per reactor, node-wide light-request p50 468 ms and p99 1.9 s while the attack
  lasts. Probes: `ra_stall.py` (matcher rows), `rc_flood.py`. Fix and test above: a unit case per shape asserting a 400 at the per-call cap and a
  live case in `lua_bounds.py` that floods K matcher requests and bounds the light GET p99.
- **X4-R2 (low): conversion holds the reactor about 0.7 s at the cap (2.7 us a value).** Probe `rb_conv.py`, `rd_sample.py`. Accept with the bound stated.
- **X4-R3 (low): tests that do not bite.** (i) The close-handler shapes use string errors, which a charged `find` ends incidentally; use `error({})` so the
  flag and the hook are what is tested. (ii) `test_print_is_bounded_per_call` only checks completion; assert the logged bytes (a counter in `host_print`
  or the log hook), since the N1 mutant survives. (iii) The 1 s foreign revalidation is only covered by the harness.
- **X4-R4 (low): semantic changes for normal scripts, all stated in the changelog except the last two.** `string.pack/unpack/packsize/dump`, `<close>`,
  `__gc/__close/__mode` and weak tables are gone; script errors now fail the write (400) instead of storing the document untransformed; `print` goes to
  the log, 4 KiB a call; `table.sort` without a comparator is a Lua merge sort (stable, no "invalid order function" error), so sorting more than roughly
  200,000 elements now ends at the instruction cap (a 1M-element sort answers 400 in about 1 s; the native sort took 0.75 s). **Not stated:** (a)
  `LUAI_MAXSTACK` went from 1,000,000 to 200,000, so `table.unpack`/`select('#', ...)` over about 200,000 elements now raises "too many results to
  unpack" (`p4a`, `p8`); (b) a script over a large document answers 413 when the document has more than 262,144 value units even if the script touches one
  field (checked: a no-op script over 100,000 and 200,000 integers answers 204, over 400,000 answers 413, while the same document sent directly answers 204).
  Add both to the changelog and `scripting.md`.
- **Low, from reading the patch:** `condemn`/`note` re-arm the count hook to 1 and back, which drops the partial period from the instruction count. A
  script can trigger that at most `max_work / slice_work` = 2,500 times, so the instruction cap can overrun by at most about 25M instructions (25%).
  Harmless; note it in the mechanism doc.

### Probes and outputs added (all in `.scratch/review-x4-recheck/`, kept)

`cfg.sh`, `runA.sh`, `runB.sh` (the original probes `p1` to `p19` rerun with `XBIN` set; outputs `out/p*.txt`, `out/progress-*.txt`), `ra_stall.py`
(`out/ra.txt`), `rb_conv.py` (`out/rb.txt`), `rc_flood.py` (`out/rc.txt`), `rd_sample.py` (`out/rd.txt`, `out/sample.txt`), `rp_print.py` (`out/rp.txt`),
`rs_params.py`, `rt_bigdoc.py`, `mut.py` (`out/mut.txt`, `out/mut2.txt`), `probe_close.cc`, `probe_close2.cc`, `probe_close3.cc` (`mut/pc_*`, `mut/pc3*`),
`drift/`, harness output `out/harness.txt`, `out/harness.json`. Run each as `quiet light NAME MINUTES WHAT -- ...` with `XBIN=.scratch/review-x4-recheck/build-rel/bin/xapiand`.

### Verdict (re-check 1)

S1 CLOSED, S2 PARTIAL (residual R1), S3 CLOSED, S4 CLOSED, N1 CLOSED, N3 CLOSED, N6 CLOSED. **LAND X4** on the condition that X4-R1 (per-call work
cap) is fixed and re-checked before any release that runs scripts from untrusted clients, and R4's two unstated changes are added to the docs; R2 is
accepted with the stated bound; R3 at the developer's discretion.

Not run: ASan, UBSan and TSan builds of the fixes (the developer reports them green; not reproduced); Linux and FreeBSD; the docs E2E (the developer's
641 of 641 not reproduced; I ran `harness/lua_bounds.py` and the probes instead); the cluster paths (a foreign script read across nodes, replicated or
restored writes running a script that now errors with 400); TLS; a fix-side measurement of the per-call cap (it does not exist yet); the idle-host
numbers (the host ran other agents' builds throughout).
