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
