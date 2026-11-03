---
title: "phase7c wide search: security review"
---

Branch `phase7c/wide-search` (worktree `Xapiand-wt/wide-search`), head `Xapiand:088eab99e`, local only. Scope: the
index-pattern expansion (`src/index_pattern.{h,cc}`, its caller `expand_paths` / `resolve_index_endpoints` in
`src/server/search_views.cc`), the boundary it builds on (`src/endpoint.cc`, `src/index_path.h`,
`docs/reviews/phase7c-gate-prep-security.md` SR1), and the cap (`--max-search-indexes`).

Threat model used: trusted cluster; the HTTP API takes untrusted client input (no authentication gate in front of the
index routes, as the gate-prep review found). The attacker is "a client that can reach the HTTP API"; "local file access"
is named where a probe plants a file.

Build: the existing Release tree `.scratch/build-rel` (`cmake --build ... --target xapiand` under `quiet light`, already
up to date at the head). Every probe starts its own node on ports 18980 to 18999 with a data directory under
`.scratch/sec-patterns/run-*/` and kills it on exit.

## Verdict: REJECT until SR1 is fixed (one blocker, small fix); SR2 and the low items are conditions

What holds: the pattern cannot reach outside the data directory, cannot list dot-led names (`.xapiand`, hidden indexes),
WAL files or quarantine directories, never follows a symlink, and the reactor is not blocked by the directory walk. What
fails: the cap is not load-bearing for the work the walk does. A single URL of about 60 KB keeps a work-pool thread busy
for tens of seconds on a 200-directory tree and for hours on a 50,000-directory one, and nothing bounds, dedupes or
cancels it.

## Findings

### SR1 (High, blocks landing) — the walk's work is bounded by neither the cap nor anything else

**Attacker:** any client that can send `GET /<items>/:search` or `:count`. **Gain:** occupies work-pool threads (the
chores that open indexes, refresh them, flush commits) for as long as the URL asks; other tenants' writes and first-time
searches queue behind it.

**Mechanism.** `index_pattern::expand` counts only the names that match and survive the exclusions
(`out_.size() > limit_`). Three things make the walk unbounded:

1. A name an exclusion removes is not counted, so `*,-*` (or any `X,-X`) reads every directory of the tree and never
   trips the cap. The cap bounds the answer, not the work.
2. Every item walks the tree again. The item list is not deduplicated and not limited: the HTTP header limit (80 KiB) and
   the path parser allow about 30,000 `*` items (`items.out`: 39,000 answers `400 Invalid path`, 30,000 are served).
   N items cost N walks.
3. The walk has no deadline and no cancellation; a client that disconnects does not stop it. `Walk::scan` also builds
   the full `children` vector of a directory before it looks at the first entry, so the cap trips only after the whole
   listing of a level is held.

**Measured** (`walk_cost.out`, `starve.out`, `starve2.out`, `items.out`, `flat.out`; macOS, Release, host load about 12,
so read the times as upper bounds on a quiet host, the ratios as the point):

| request | tree | result |
|---|---|---|
| `*,-*` (1 item) | 50,000 index dirs | 0.53 to 0.60 s on one worker |
| 100 / 1000 items of `*` plus `-*` | 50,000 | 52.7 s / 518.7 s on one worker |
| 30,000 items of `*` plus `-*` (60 KB URL) | only 200 | 42.9 s on one worker (about 1.4 ms per item); on 50,000 dirs it is about 4 hours |
| `*,-*` | 300,000 | 8.3 s |
| `_all` (cap trips at 100,001) | 300,000 | 4.4 s to answer 400 (the whole level is listed first); 8 at once peak at +152 MiB, about 19 MiB per walk (58 bytes per entry) |
| 40 requests of 40 items of `*` plus `-*` | 20,000 | a `PUT /live1/d1` made after them waited **36 s** (`starve2.out`); `GET /` stayed at 0 ms (the reactor is free, the work pool is not) |

**Why it blocks.** The mechanism doc and the ledger state the walk "stops at the cap instead of listing" a million
directories. That is true only for a pattern whose matches are all kept. It is the property the lead asked about
(exclusions and the cap), and the "millions of indexes per host" workload makes the base walk cost real, not
theoretical.

**Fix direction** (the developer picks; each part is small):

1. Count every directory entry the walk visits against a budget (for example a multiple of the cap, or its own option),
   400 naming it, so the work has a number. Exclusions then cannot defeat it.
2. Walk once for the whole item list (test each name against all wanted patterns) or deduplicate identical items, and
   bound the number of items (the cap is about names; a hundred or so items is enough for any real request).
3. Read a directory incrementally (stop at the budget mid-`readdir`) instead of materialising `children`.
4. Check the request for cancellation between directories, or give the walk a deadline.

Probes: `walk_cost.py`, `starve.py`, `items.py`, `flat.py`.

### SR2 (Medium, condition) — any client can make other indexes vanish from a wide search

**Attacker:** a client with ordinary create rights (a PUT). **Gain:** silent suppression of other indexes in `_all` and
prefix patterns; the response is a clean 200.

An index directory is not descended (`holds_index` then stops the walk). So creating the index `tenant`
(`PUT /tenant/d`) removes `tenant/a` and `tenant/b` from `_all`, `tenant*` and `tenant/*`; creating `tenant/strata`
does the same (a container with a child `strata` looks like a 1-shard index). Both measured (`hide.out`, `hide2.out`;
5 shards and 1 shard behave alike; the hidden indexes stay searchable by name). The indexes are not damaged
(`collide.out`: `x/strata` created next to the 1-shard index `x` leaves `x` intact after a restart), they are only
invisible to patterns. The mechanism doc states that an index is not searched for indexes below it, but does not say that
a client can use that to blind the others.

Fix direction: descend into an index directory too, skipping the names the layout reserves (`strata`, `wal.*`,
`.__N`, dot-led), or refuse at `Endpoint` the creation of an index whose path is below an existing index or whose
component is `strata`/`.__N`. At least state it in `wide-search.md` and the API docs as a property of wide searches.
Probes: `hide.py`, `hide2.py`, `collide.py`.

### SR3 (Low) — a cap-sized pattern holds a loop for seconds (attribution open)

A pattern expanding to exactly the cap (100,000 names, `i0000*` over fake index dirs) took 9.7 s and stalled one of the
reactors for a worst ping of **1.76 s** (2.8 s with four at once; idle worst 2 to 3 ms) (`stall.out`, `stall4.out`).
The stall is the per-name loop of `resolve_index_endpoints` and the open of each shard on the reactor. A `sample` of the
busy loop (`sample_stall.txt`) shows the time in `ShardEndpoint::ensure_open` rethrowing, which is an artifact of my
fake (shardless) index directories, so I did not attribute it to the pattern code; the 10,000-real-index gate (the
lead's) is the check for real indexes. Keep an eye on it when SR1 lowers the budget; yielding every few hundred names
in that loop would bound the stall. Probe: `stall.py`, `sample_stall.py`.

### SR4 (Low) — an exact duplicate or an overlap is counted twice against the cap

The names go into one vector before the sort/unique, so `a*,a*` over 100 indexes counts 200 and answers 400 at cap 100
(`cap.out`). Conservative, never a bypass. Counting after a dedupe (or per distinct name) removes it.

### SR5 (Low) — `--max-search-indexes 0` removes the cap and so every bound in the walk

The code reads 0 as "no cap" (`std::numeric_limits<size_t>::max()`); the option help does not say so. With SR1 fixed by a
budget derived from the cap, 0 must still leave a walk budget.

### SR6 (Info) — wide search is a name-enumeration and authorisation surface

A deployment that puts an authorising proxy in front and decides by the first path segment (`/tenantA/...`) now has to
treat `,`, `*`, `_all` and a leading `-` as multi-index syntax: `_all` reads every index, and `:count` on a prefix
enumerates names by existence (`/ab*/:count`). Nothing leaks a path (hits carry no index name); the exposure is that an
unknown name no longer protects an index. Worth one line in the API docs.

### SR7 (Info) — a damaged index is silently skipped (not attributed to this branch)

A directory `corrupt/.__1/strata` with a garbage manifest gives `200` with zero hits both by name and under `_all`, and
nothing in the node log (`broken.out`). A wide search over a damaged index returns partial results without a signal.
I did not run master to see whether the plain-name path always did this.

## Probed and clean

Each row is one suspicion with its probe; "clean" means the server was observed doing the safe thing.

| Suspicion | Probe | Result |
|---|---|---|
| `..`, `../*`, `*/../other/*`, `%2e%2e/*`, `..%2fother%2f*`, absolute `//abs/path/*` and its encoded forms, `~`, `$HOME` reach another node's real data directory (a second node, `parent/other`, holds `idx=other`) | `traversal.py` (`traversal.out`, 90 vectors on `:search` and `:count`) | clean: no hit ever carried `idx=other`; `..` alone and `//abs` answer 400 "empty or outside the data directory" (the router also resolves `../` and `./` before the view; `../*` is `*`) |
| NUL, backslash, full-width dots and slashes, overlong UTF-8, RLO in a pattern | same | clean: 200 with no hits, or 400 "holds a byte a name cannot hold" when the item is a plain name beside a wildcard; no crash (the gate-prep SR1 class) |
| Symlinks: `sym -> other node`, `symparent -> parent`, a container whose `strata` is a link (planted by someone with file access; no API creates a link) | `traversal.py` | clean: never listed (`DT_LNK` is skipped, `DT_UNKNOWN` is `lstat`ed); nothing from the link target was read. A link named `strata` inside a container makes the container "an index" by name only; the later open reads `name/.__N`, which does not exist |
| `.hid`, `.xapiand`, `.xapiand/indexes`, `*xapiand*`, `.*` | `traversal.py`, `explore.py` | clean: dot-led names are skipped at every depth; a hidden index is still searchable by naming it (documented) |
| WAL / quarantine directories | code and layout (`layout.py`): `wal.0` is a file inside the shard directory, quarantine is `.quarantine.XXXXXX` inside it, and an index directory is never descended | clean |
| Partial index (`partial/.__1` empty), a regular file, a FIFO, `bad\nname`, `bs\x`, `q?x`, `c:x`, `-lead`, `*star`, `a,b` as directories | `traversal.py`, `broken.py` | clean: a partial index is listed and the search skips it with no error; files and FIFOs are not listed; names with control bytes or backslash fail `index_path::valid` and are skipped. (APFS refuses invalid UTF-8 names; the filter is the same `valid`, covered by the fuzz below) |
| Names the API creates that `Endpoint` re-parses (`q?x`, `c:x`, `a#b`, `https:`, `file:`, `x:/y`) | `oddnames.py` | no new effect: `PUT /q%3Fx` writes into index `q` as before; every directory listed came back under its own name |
| The pattern on a write route (`DELETE /*`, `/_all/d`, `/a*/`, `PUT /a*/d2`, `:commit`, `:dump`, `:restore`, `:check`, `:wal`, `:delete_by_query`, `:update_by_query`, `:truncate`) | `destructive.py` (`destructive.out`) | clean: `patterns` is set only for search and count; everything else treats `*` and `_all` as a literal name and every index survived |
| Catastrophic backtracking | code, `fuzz_index_pattern` (1.25 M runs, the lead's) | the matcher keeps only the last star (O(pattern x name)); 256-byte pattern x 255-byte name is 65 k steps. No exponential path |
| `below()` pruning loses a match, or a deep tree | `diff_expand.py` (4 seeds, 833 random patterns over 40 real indexes, 0 mismatches), `fuzz-expand/` (below), `deep.py` | clean. Deep trees: the API refuses a path over its keyword limit ("Keyword too long", about 100 levels accepted), and a 447-level chain made by `mkdir` is walked in 0.11 s without a crash; the walk stops at `PATH_MAX` because `opendir` fails |
| Cap interplay with exclusions | `cap.py` (`cap.out`) | the answer is bounded: `_all` over 150 at cap 100 is 400 for search and count; an exclusion that leaves 100 passes; 100 plain names plus one is 400 with the count. The work is not bounded (SR1) |
| Error text discloses a path | `traversal.py`, `cap.py`, `misc.py`, `broken.py` | clean: the 400s echo the client's own item only (`Index path '..' is empty ...`, `An index pattern is at most 256 bytes`, the cap text names the option and a count); no data-directory path in any response or in the log lines grepped |
| Pattern length | `misc.py` | 256 bytes is served, 257 is 400 (also for a `-x` item and a plain item beside a wildcard) |
| Reactor blocked by the walk | `async_check.sh --strict --owed --extern strata=<the strata archive at 3f564dc5>` (`.scratch/sec-patterns/strata-pin`): 255 hits, allow 242, owed 0, compiled_out 13, unclassified 0, exit 0; every `index_pattern.cc` syscall is classified as the query chore; `starve.py`/`walk_cost.py`: `GET /` 0 ms during 8 to 40 busy walkers | clean (the work pool is the shared resource, SR1) |

## Fuzz extension

The shipped target covers `match` only. I added `.scratch/sec-patterns/fuzz-expand/fuzz_expand.cc`
(libFuzzer + ASan + UBSan, LLVM from `/usr/local/opt/llvm`), which drives `expand` itself against a fixed on-disk tree
(12 indexes, a nested one, a dot-led index, a container with a dot-led index, an empty chain, and two symlinks, one to `/`)
with a random limit and up to 8 items. It asserts: no crash or sanitizer report; the result is sorted and unique; every
wildcard-derived name is valid, an index of the tree, with no leading `/` or `.`; the result equals a brute-force oracle
(`match` over the tree minus exclusions, plus the plain items); a throw happens only when the oracle exceeds the limit.
300,000 runs in 241 s, no finding. It is built from a copy of the head's `index_pattern.cc` (`index_pattern_copy.cc`,
`copy_of_head.txt`) with a one-header stub (`stub/xapiand_strings.hh`) because the real header pulls the whole tree. The
developer can lift it into `tests/` and CMake next to `fuzz_index_pattern`; SR1's fix should add a mutant-killing case
for the work budget there.

## Not covered by the table

- `Walk` on a filesystem that returns `DT_UNKNOWN` for everything (the `lstat` branch): read, not run.
- Linux and FreeBSD: nothing platform-specific found in the code (`opendir`, `readdir`, `d_type`, `lstat`); not run.
- A race between listing and open (a directory swapped for a link after the walk): needs local file access; the open
  path is the earlier boundary's (`Endpoint`), unchanged here.

## Probes kept (`.scratch/sec-patterns/`)

`sec_lib.py` (node launcher and raw HTTP), `layout.py`, `explore.py`, `traversal.py` / `traversal.out`,
`diff_expand.py` / `diff_expand.out`, `walk_cost.py` / `walk_cost.out`, `starve.py` / `starve.out` / `starve2.out`,
`items.py` / `items.out`, `misc.py` / `misc.out`, `flat.py` / `flat.out`, `stall.py` / `stall.out` / `stall4.out`,
`sample_stall.py` / `sample_stall.txt`, `deep.py` / `deep.out`, `broken.py` / `broken.out`, `oddnames.py` /
`oddnames.out`, `cap.py` / `cap.out`, `hide.py` / `hide.out`, `hide2.py` / `hide2.out`, `collide.py` / `collide.out`,
`destructive.py` / `destructive.out`, `fuzz-expand/` (target, copy of the file under test, stub, corpus), `strata-pin/`
(the archive of strata `3f564dc5` for `async_check`). The large generated data directories of the cost probes
(`run-flat`, `run-cost`, `run-starve`, `run-misc`) were removed; the scripts rebuild them (`flat.py 300000 8` takes 75 s to
create its tree).

## Verdict

REJECT for landing until SR1 is closed. No other finding blocks. Conditions: SR2 fixed or stated in
`wide-search.md` and `api.md`; SR4, SR5, SR6 folded into the SR1 fix or the docs; SR3 checked at the 10,000-index gate with
a loop-latency probe (`stall.py` against real indexes); `fuzz_expand` lifted into the tree with the SR1 mutant.

Blocking items:

1. SR1: the walk's work is bounded by neither the cap nor an item count (exclusions and repeated items defeat the cap; no budget, dedupe, or cancellation).

Not run: Linux and FreeBSD; ASan/TSan builds of the server (the fuzz target ran under ASan+UBSan); the 10,000-real-index
gate; master as a baseline for SR7; Windows-style or case-insensitive filesystems.
