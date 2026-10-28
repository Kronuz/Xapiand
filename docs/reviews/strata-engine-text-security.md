# Security review: phase7a/strata-engine-text (S4, queryparser as the host text parser)

Branch `phase7a/strata-engine-text`, head `Xapiand:b1b03ea62`, worktree
`~/code/Kronuz/Xapiand-wt/strata-text`. Reviewed against the ledger
(`docs/status/strata-engine-text.md`), the mechanism note
(`docs/mechanisms/text-queries.md`), and the diff from merge-base `5d7f3263d`. Clean room
honored: did not open `src/xapian/`, `oracle.cc`, `sortable_serialise.{cc,h}`, or
`src/server/remote_protocol_views.cc`. No `/tmp` use; all probes and harnesses are under
`.scratch/s4-sec/` in this worktree (kept, not deleted; see list at the end).

## Threat model

Attacker: a network client with valid credentials (or no credentials, if the deployment runs
without auth, which the probes below did) who can write documents to a text field and send
search requests — i.e. anyone who can use the HTTP search API. Goal of the review: can such a
client crash the process, exhaust memory/CPU/stack disproportionately to the request size, or
get a 500 instead of a 400 from a malformed query. The ledger's own stated goal is that the
*lowering* (`engine::parse_text` / `src/engine/text.cc`) is iterative and bounded, closing the
heap overflow the vendored `Xapian::QueryParser` had on `_near` over a bracketed boolean
expression. That specific input (`(barack OR michelle) AND obama` under `_near`) is confirmed
fixed: it now returns 400 via `QueryParserError`.

## SR1 (critical, blocking): deeply nested boolean text crashes the process via `to_xapian`'s recursive combine conversion, and the live server crashes even on the HTTP path

**Attacker:** any network client (with or without auth) who can PUT a text document and run a
search. **Gain:** reliable process crash (stack overflow / `SIGSEGV`) with one HTTP request of a
few hundred KB, no special privilege needed.

`engine::parse_text`'s lowering (`convert()` in `src/engine/text.cc`) is genuinely iterative, as
the ledger claims — confirmed with `test_nesting_is_not_recursive` and independently with a
200,000-deep paren text and an iterative-shape probe. But the *lowered* `engine::Query` is a tree
of `shared_ptr<QueryNode>` as deep as the input text, and both consumers of that tree recurse:

- `engine::xapian::to_xapian()` (`src/engine/xapian/query.cc:95`, unchanged by this branch)
  recurses on `node->subqueries[0]`/`[1]` for `QueryKind::combine`. It only flattens the
  `QueryKind::append` chain iteratively; the rest is plain recursion.
- `Query::description()` (`src/engine/query.cc`) likewise recurses for `combine`.

The mechanism note's own "Limits" section says so ("the lowered tree is as deep as the text, and
`to_xapian` and `Query::description` recurse over it") — but frames it as a cost, not as a crash.
It is a crash. Measured with a standalone harness built against the same objects as
`xapiand_fuzz_text` (`.scratch/s4-sec/scale_s4.cc`, `.scratch/s4-sec/build_scale.sh`):

```
altparen n=2000  -> to_xapian ok   (9001 nodes)
altparen n=5000  -> to_xapian ok  (10001 nodes)
altparen n=20000 -> AddressSanitizer: stack-overflow in Xapian::Query::Query(...) via to_xapian query.cc:95
```

(`.scratch/s4-sec/p1b_bisect.py` bisects the exact depth against the live ASan server over HTTP,
sending a fresh `SEARCH /t/?sort=_id` with `{"body": "a AND (" * n + "c" + ")" * n}` on a text
field, no `_near` needed — plain boolean text crashes it too):

```
andparen (identical-op chain): survives depth 19062, dies at depth 20000  (~140 KB body)
altparen (alternating AND/OR): survives depth 38125, dies at depth 40000  (~300 KB body)
```

`node-p1.log` and `node-p2.log` in `.scratch/s4-sec/` show the live-server crash is a stack
overflow, sometimes caught inside `to_xapian` itself, sometimes one layer further inside Xapian's
matcher (`GlassPostList::get_docid_range` / `OrPostList::get_docid_range`, called from
`AndContext::postlist` during `get_mset`) — I did not open `src/xapian/` to diagnose that second
site; I only observed the ASan backtrace it printed. Either way the HTTP request that triggers it
is ordinary JSON with a text field, no `_near`, no exotic flags, and well under any request-size
limit this review found (none was found at all — see SR2).

**Why this is new, not a restatement of the already-closed Xapian heap overflow:** before this
branch, a text field's query string went straight into `Xapian::QueryParser::parse_query()`
(`src/engine/xapian/query.cc`'s now-deleted `parse_text(const QueryNode&)`), which built its own
`Xapian::Query` tree directly and never touched `engine::xapian::to_xapian`'s recursive
`QueryKind::combine` case — a text node was one opaque `QueryKind::text` leaf. After this branch,
the *same* nested-boolean text input is first lowered (safely, iteratively) into an
`engine::Query` tree and then *must* pass through `to_xapian`'s recursive combine conversion to
reach Xapian at all. S4 made that recursive conversion newly reachable, at full nesting depth,
from a text field's free-text query string — and at a much lower bytes-per-level cost than the
only pre-existing way to reach the same `to_xapian` recursion (nested JSON `_and"` in the query
DSL, see SR2): `"a AND ("` is 7 bytes per level of text; `{"_and":[...]}` is roughly 4x that in
JSON. The design's stated goal — "the lowering must stay iterative and bounded" — holds for the
parse step and fails for the conversion step it feeds. `test_nesting_is_not_recursive` only
proves the former (it calls `engine::parse_text` and checks `CHECK_FALSE(lowered.empty())`; it
never calls `to_xapian` or `description()` on the deep tree it builds), so nothing in the test
suite would have caught this gap.

**Recommend:** make `to_xapian`'s `QueryKind::combine` case iterative the way `QueryKind::append`
already is (an explicit worklist, same shape as `engine::text.cc::convert()`), or cap the nesting
depth during lowering and raise `QueryParserError` (400) past a documented bound, with a
regression test pinned just below whatever bound is chosen. Either fix should also cover
`Query::description()`, which recurses the same way and is on the `L_QUERY`/`L_DEBUG` logging
path (`src/query_dsl.cc:1249`, `src/database/handler.cc:1031`) — reachable with the same input
whenever that log category is on.

Probes kept: `.scratch/s4-sec/scale_s4.cc` + `build_scale.sh` (standalone `to_xapian` depth
bisection, no server), `.scratch/s4-sec/p1_nesting.py` (shape library), `.scratch/s4-sec/p1b_bisect.py`
(binary-searches the crash depth against a live node), `.scratch/s4-sec/node-p1.log`,
`.scratch/s4-sec/node-p2.log` (ASan backtraces).

## SR2 (critical, not introduced by this branch — pre-existing on master, already named as a trap in AGENTS.md): `QueryDSL::process` recurses over the JSON query DSL with no depth cap

**Attacker:** same as SR1. **Gain:** process crash with an ordinary JSON body, independent of any
text parsing — no `q=`, no text field, just nested `{"_and": [...]}`.

```python
# a few hundred levels of nested _and, no text parsing involved
{"_query": {"body": {"_and": [{"_and": [... {"_value": "a"}]}, {"_value": "a"}]}}}
```

Confirmed crashing between depth 200 and depth 1000 (`.scratch/s4-sec/p6_dsl_nest.py`,
`.scratch/s4-sec/node-p6.log`): `ASan stack-overflow` inside `MsgPack::_init_map` /
`__hash_table::__do_rehash`, called from `QueryDSL::process` (`src/query_dsl.cc:202` ×
`:408`/`:269`/`:355` recursing into itself). This is unrelated to `engine::parse_text` or
`queryparser` — `QueryDSL::process` is untouched by this branch's diff (the merge-base diff of
`src/query_dsl.cc` is the 4-line call-site swap to `engine::parse_text`) — and it is already
named as a known trap in this worktree's `AGENTS.md` ("`query_dsl.cc::process` recurses over the
query object with no depth cap... be careful adding nesting"). I am flagging it because it sits
on exactly the attack surface this review was scoped to (query DSL `_text`/`_near`/`_phrase`/`q=`)
and because `q=` reaches it too (via `BooleanTree`/`make_dsl_query` building the same kind of
nested MsgPack before handing it to `process`), but it does **not** block this branch — fixing it
is separate work, tracked here for visibility, not as a condition of this merge.

Probes kept: `.scratch/s4-sec/p6_dsl_nest.py`, `.scratch/s4-sec/node-p6.log`.

## Other probes, no finding

- **Wildcard/edit-distance expansion limits:** `TextConfig` sets `partial_max_expansion` (50) and
  `fuzzy_max_expansion` (0, i.e. error-on-exceed) but has no `wildcard_max_expansion` field at
  all, so a plain `aa*` or `_wildcard` query on a text field has `max_expansion = 0` (no cap) —
  confirmed in `src/engine/text.cc` (`cfg.wildcard_expansion` is never assigned) and in
  `.scratch/s4-sec/scale_1.out` (no crash, no runaway memory up to a million-byte pattern, but
  also no cap enforced by the host). This is **unchanged by this branch**: the deleted
  `src/engine/xapian/query.cc::parse_text(const QueryNode&)` also never called
  `set_max_expansion(..., FLAG_WILDCARD)`, only `FLAG_PARTIAL`/`FLAG_FUZZY` — confirmed in the
  diff. Non-blocking; worth a follow-up to decide whether text-field wildcards should get a host
  cap the way `partial`/`fuzzy` already do, independent of S4.
- **`?`/`*`-only patterns:** the all-wildcard match-all ruling (`leaf()` in `src/engine/text.cc`)
  and its boundary (`**`, `*?`, `?*` are match-all; `??`, `*??` are not) matched the ledger's
  decision and its own test; did not find a bypass.
- **Deep nesting via `_near`/`_phrase` specifically:** rejected at parse time with 400
  (`QueryParserError`, "OP_NEAR and OP_PHRASE only currently support leaf subqueries") well before
  any depth matters — confirmed at depths 1,000 through 20,000. The original reported bug
  (`_near` over `(barack OR michelle) AND obama`) stays fixed.
- **Invalid UTF-8, NULs, overlong/surrogate sequences, combining-character runs:** fed as raw
  bytes via msgpack, raw JSON bytes, and JSON `\uXXXX` escapes (including lone surrogates) to
  plain text, `_near`, `_wildcard`, `_and`, and `q=`, against both a plain text field and a
  `cjk_ngram` field. No crash, no 500; lone high surrogates are correctly rejected at the JSON
  parse stage (400, bounded message); everything else that parses returns 200 with sane results.
  Probes: `.scratch/s4-sec/p5_utf8.py`, `.scratch/s4-sec/p45.out`.
- **Error paths:** a short grammar+byte fuzzer against `_near`/`_phrase`/`_and`/`_or`/`_wildcard`/
  `_fuzzy`/`_partial`/`q=` for 150s (`.scratch/s4-sec/p4_errors.py`) found every response either
  200 or 400 with a bounded body, consistent with `src/server/http_utils.h`'s dispatch (any
  `errc::syntax` or `ClientError` is a 400 with a bounded message; nothing else leaked a stack
  trace or secret). One **unreproduced** SEGV surfaced once in ~26,000 requests of that run,
  inside Xapian's own `EstimateOp`/`SmallVector` code
  (`Xapian::Vec<EstimateOp*,...>::clear()`, called from `AndContext::postlist`) — I did not
  diagnose it further since it sits inside `src/xapian/` (clean-room) and I could not isolate a
  minimal repro before the time box; noting it for whoever re-runs
  `.scratch/s4-sec/p4_errors.py` (seed 1, ~150s) to watch for recurrence, not as a confirmed
  finding.
- **`fuzz_text` (ASan+UBSan), 450s across 5 seeds (1–4, 101–103), ~2.7M texts total:** no
  violation beyond the already-recorded A/B decisions; UBSan hits are only the pre-existing
  null-offset-arithmetic in `lz4.c` and RapidJSON's `stack.h` (third-party, not the text path, as
  the ledger already notes).

## Not run

- `docs/reviews/calibration.md` does not exist in this repository (checked both the worktree and
  the base clone); proceeded without it.
- Did not rebuild master at the merge-base to get a byte-for-byte differential crash depth for
  SR1 against the pre-S4 `Xapian::QueryParser` path — the clean-room rule plus the time box made
  that not worth another full ASan build; the code-path argument above (text no longer bypasses
  `to_xapian`'s combine recursion) stands on the diff alone.
- Did not pursue the p3 expansion-cost probe to a real high-cardinality index: the harness
  returned `total=0` for every query including the exact-term baseline, which reads as a harness
  bug (schema/commit timing), not a product one; left `.scratch/s4-sec/p3_expansion.py` for a
  re-check to fix and rerun if expansion cost under real cardinality still matters.
- No TLS/cert/suite review: this branch touches no transport code.

## Probes and harnesses kept (`.scratch/s4-sec/`)

`lib.py` (shared HTTP/server helpers), `build.sh`/`build.log`/`build.out` (the ASan+UBSan build,
own deps, under `quiet light`), `build_scale.sh`/`scale_s4.cc`/`scale_s4` (standalone `to_xapian`
depth harness, linked against the fuzz target's objects), `p1_nesting.py` (nesting/length shape
library over HTTP), `p1b_bisect.py` (binary-searches the live crash depth), `p2_httpq.py` (`q=`
path), `p3_expansion.py` (wildcard/fuzzy expansion cost — inconclusive, see Not run),
`p4_errors.py` (grammar+byte error-path fuzzer), `p5_utf8.py` (UTF-8/NUL/surrogate edge cases),
`p6_dsl_nest.py` (direct JSON `_and` nesting, SR2), `queryparser-09a4ea6.cc` (reference copy of
the pinned commit, for offline reading only — not opened anything under `src/xapian/`),
`node-p*.log` (ASan/UBSan server logs with the backtraces cited above).

## Verdict

**APPROVE WITH CONDITIONS.**

Blocking (must close before merge):

- **SR1** (critical): a text field's plain (non-`_near`) nested boolean query crashes the process
  via `to_xapian`'s recursive combine conversion — confirmed over HTTP at ~140–300 KB request
  bodies with no special flags. This reopens the crash class the branch's headline fix closed,
  through a path S4 itself newly made reachable. Fix `to_xapian` (and `Query::description()`) to
  stay iterative over `combine`, or cap lowering depth with a 400, plus a regression test.

Non-blocking (tracked, not conditions of this merge):

- SR2: pre-existing, uncapped recursion in `QueryDSL::process` over nested JSON query DSL — same
  attack surface, different code, not touched by this branch.
- No host-side cap on text-field wildcard expansion (unchanged from pre-S4 behavior).
- One unreproduced SEGV inside Xapian's matcher internals during error-path fuzzing; needs a
  repro before it's actionable.
- `p3_expansion.py` harness bug (not a product finding) — fix and rerun if expansion cost under
  real term cardinality is still an open question.


## Re-check 1 (head `Xapiand:4233bf25e`, fixes `Xapiand:3612b7a57` and `Xapiand:4233bf25e`)

Clean room honored (no `src/xapian/`, `oracle.cc`, `sortable_serialise.{cc,h}` or
`src/server/remote_protocol_views.cc` opened; the Xapian frames below are ASan backtrace names
only). ASan+UBSan build in its own dir (`.scratch/s4-recheck/build`, own deps, deleted after),
probes in `.scratch/s4-recheck/` (`p7_recheck.py` groups text/flat/dsl/qstr/body/yaml,
`p8_width.py`, `p9_mp.py`, outputs `p7_*.out`, `p8_*.out`, `p9_mp.out`, ASan logs `node-r*.log`).
Nothing outside this file was edited; no commit.

### Verdicts

- **SR1: PARTIAL.** The nesting crash I reported is closed, but a width crash through the same
  tree remains, so the claim "every case past a cap answers 400" and the cap's rationale do not
  hold. See SR1b.
- **SR2: CLOSED** for `QueryDSL::process` and the `q=` boolean path (below). The MessagePack and
  YAML body nesting the developer flagged is a crash, filed as SR3 (blocking for the claim, not
  for SR2's own code).

### What is closed (SR1 nesting, SR2)

- `to_xapian` is a post-order loop on an explicit stack (`src/engine/xapian/query.cc:141-185`),
  `Query::description()` likewise (`src/engine/query.cc:239-313`). No other recursion over the
  tree exists: `census.cc:235-278` already uses a worklist, and `~QueryNode`
  (`src/engine/query.cc:80-98`) frees chains in a loop. The 40 KB-stack test
  (`tests/test_engine_query.cc`, `test_depth_cap`) builds, describes and converts a tree at the
  cap.
- The caps live in `set_extent` (`src/engine/query.cc:58-77`), called from every factory that
  builds an inner node (`combine` x2, `append`, `scale`); leaves (`term`, `wildcard`,
  `edit_distance`, `source`) have depth/size 1, and `QueryNode` is built nowhere outside these
  factories (grep of `src/`). Every caller found goes through them: text (`text.cc:229-236`),
  DSL (`query_dsl.cc:210,410,424,430,736`), `multivalue/range.cc`, `generate_terms.cc`,
  `geospatialrange.cc`, `search_views.cc:2302`. `QueryDepthError` derives from `QueryParserError`
  and is a `ClientError`, so it answers 400. The caps fire when the node is built (bottom-up),
  before `to_xapian` or any matcher sees it, and `text.cc:218` additionally refuses at the push
  of level 101 before the rest of the text is lowered. Aggregations build no queries (no
  `engine::Query` use under `src/aggregations/`). The remote/replication side builds
  `Xapian::Query` in `src/server/remote_protocol_views.cc`, which the clean room keeps me out of;
  I did not review it (cluster-internal peers; see "For the lead").
- Probes against the live ASan server, all alive, no 500, no crash:
  - Text with the boolean modifier (`_or`/`_and`/`_near`), shapes paren/altparen/andparen/orparen/
    notchain/minus/plus/wcflat/tilde at n = 50..200000: 200 up to depth 99, **400 from 100**
    (`p7_text.out`; same-operator chains and bare parens flatten and answer 200 by design).
  - The original SR1 shapes (`a AND (` x 20000 and 40000, 140-300 KB): 400 in 0.00-0.01 s
    (`p7_text.out`, `harness/query_depth_check.py`).
  - Flat width: 4095 words 200, 4096/4097/12000/14000/100000 words 400 for plain, `_and`, `_or`,
    `_near`, `_phrase`, `_wildcard`, `_partial`, `_fuzzy` and boolean AND/OR/alternating
    (`p7_flat.out`). One 100000-word `_fuzzy` request timed out at 300 s with the node alive (a
    `_fuzzy` over 100 KB of words spends its time before the cap is reached; see "For the lead").
  - DSL: `_and`/`_or`/`_filter`/`_not`/`_scale`/`_xor` arrays, object-in-object and array-of-array
    nesting at 20..12000 levels: nesting 44 answers 200, 45 answers 400; array-of-array is capped
    at the frame limit (n=150 answers 400); field paths 30..12000 answer 400; 4095 values 200,
    4096+ 400; 40 levels x 100 leaves 200, x 110 leaves 400 (`p7_dsl.out`).
  - `q=`: altparen / NOT / parens / xor-maybe / right-deep chains at n = 20..1000 answer 200 or
    400 correctly (n = 44-46 alt and NOT chains answer 400); at n >= 20000 the URL is too long and
    the connection is reset (transport limit, node alive) (`p7_qstr.out`).
  - `harness/query_depth_check.py` against the new build: exit 0, every line ok.
- Unit tests: `xapiand_test_engine_query` 22411 checks and `xapiand_test_text` 13196 checks pass
  under ASan+UBSan (the only sanitizer notes are the known lz4/rapidjson null-offset hits).
- **Revert check:** with the two cap checks in `set_extent` disabled, `test_depth_cap` fails
  (6 of 22411 checks, `tests/test_engine_query.cc:952,958,959` among them); with the
  `json_depth` guard in `json_load` disabled, `test_text` dies with a rapidjson `ParseArray`
  stack overflow (`test_json_depth_matches_the_document`). Both restored; `git status` is clean.
- Caps before recursion: `json_load` scans the text for depth first (`src/database/utils.cc:147-179`,
  limit 200) and so refuses before rapidjson's recursive `ParseArray`; `expression_levels`
  (`src/query_dsl.cc:956-1015`) refuses a `q=` expression before `make_dsl_query` builds the
  nested object; the `Depth` guard in `process` (`src/query_dsl.cc:192-202`) counts frames, and
  the guard's constructor throws before incrementing so the counter stays balanced. `QueryDSL`
  is built per request (`src/database/handler.cc:1015`), so `depth` is not shared across
  threads.

### SR1b (critical, blocking): AND of a few hundred phrases crashes Xapian's matcher below the clause cap

The 4096-clause and 100-level caps bound node count and depth, but Xapian's matcher recurses
(`EstimateOp::resolve`, `estimateop.cc:312`, 250+ frames in the backtrace) per ANDed operand
that is itself an operator, so the node count is the wrong bound. Reproduced over HTTP,
node killed by an ASan stack overflow, body 4-5 KB:

```
text "_and": '"a b" ' x n      n=600 -> 200, n=650 -> crash   (p8_bis2.out, node-r8phrase_and*.log)
text boolean: "a b" AND ... x1365 -> crash                      (p8_phrase_bool.out)
DSL {"_and":[{"_phrase":"a b"} x n]}   n=600 -> 200, n=1365 -> crash   (p8_dsl.out)
DSL {"_and":[{"_near":"a b"} x 1365]}  -> crash                  (p8_dsl.out)
```

1365 phrases is 4095 nodes, under the clause cap; the crash starts between 600 and 650 phrases
(about 1,800-1,950 nodes) under ASan at -O1. The same width of other shapes survives: AND of
4095 terms, 4095 fuzzy leaves, 4095 wildcard leaves, 1365 `(a OR b)` groups, 1365 `(a AND b)`
groups, 1365 `(a NOT b)` groups, an OR of 1365 phrases (`p8_*.out`), so the bound depends on the
operand kind and the developer's "survives 12,000 flat terms" measurement does not carry over.
This is almost certainly the "unreproduced SEGV in `EstimateOp`/`AndContext::postlist`" my first
review recorded; it is now reproduced and reachable from a 4 KB request. It is not new to S4 (the
same Xapian tree was reachable before), but SR1 asked for the matcher to be protected and the
fix's stated margin ("500 levels survive, caps at 100 and 4096") is wrong for this shape.

Fix direction (developer's call): bound operands of AND-like operators that are themselves
operators (phrases, near groups) at a width the matcher survives with margin (e.g. weight an
operator child at 8 or more clauses, or cap AND fan-out of non-leaf children near 100), then
re-measure every operand kind (AND/FILTER/AND_NOT/XOR/MAYBE/NEAR/PHRASE x term/phrase/group/
scale/synonym/wildcard) with a bisect to the crash, add the worst shape to
`harness/query_depth_check.py`, and keep a 2x margin under ASan.

### SR3 (critical, blocking for "every case past a cap answers 400"; pre-existing, not in S4's code): deeply nested MessagePack and YAML bodies crash the node

`json_load` is the only body decoder with a depth guard. The other decoders in
`search_views.cc:2866-2870` (`yaml_load` then `MsgPack(rdoc)`, `MsgPack::unserialise`) have none:

```
MessagePack array nest (0x91 x n)  PUT/SEARCH: n=600 -> 400, n=800 -> crash   (p9_mp.out)
MessagePack map nest   (0x81 0xa1 f x n)  PUT: n=250 -> 204, n=400 -> crash
YAML flow sequence "[" x 1000      PUT/SEARCH -> crash                        (p7_yaml.out)
```

The MessagePack crash is `msgpack::object::object(const object&, zone&)` (object.hpp:570, copy
recursion), reached with a 1 KB body; no credentials needed. YAML at 150 levels answers 400/500,
at 1000 levels kills the node (the later YAML cases did not run after the node died).
Also note the access-log prettifier (`search_views.cc:3195-3220`) decodes the same bodies again,
inside a `try` that cannot catch a stack overflow.

Fix direction: a depth scan that rejects before decoding, as `json_depth` does, for MessagePack
(walk the header bytes iteratively) and YAML (flow-bracket and indentation depth, or a limit on
libyaml events); one shared limit; unit tests with a small-stack thread as in `test_depth_cap`.

### Developer-flagged items

- 150-level-deep field path answers 500 (`JSON PUT object nest n=150` and `YAML flow map nest
  n=150`): **wrong status only**, node alive, schema code rejects it with `InvalidArgumentError`
  (`p7_body.out`). The same shape at 190/199 answers 400. Fix when convenient: map that error to a
  400. Not blocking by itself.
- MessagePack and YAML body nesting: **crash**, SR3 above.

### For the lead (one line each, outside scope)

- `src/server/remote_protocol_views.cc` builds `Xapian::Query` from peer messages; not checked
  (clean room). Trusted-cluster model applies, but a depth/width check there is worth a look by
  someone allowed to open it.
- An aggregation nested 10 levels (`_aggs` of `_terms` on one field) did not answer within 120 s
  (node alive); not diagnosed, may be my payload or a real hang.
- `q=` with an AND chain of 1000 terms costs 3.4 s CPU under ASan (0.1 s at 100): superlinear in
  `make_dsl_query`; a few thousand terms fit in a URL, so this is a CPU cost to keep an eye on.
- A `_fuzzy` text of 100000 words spends minutes before the clause cap applies (3-4 s at 12000-
  14000 words); the cap check could come before the per-word expansion.
- The new caps also reject large legitimate queries (more than 4096 values in an `_in`/`_or`
  list); intended, but worth a line in the changelog.
- DSL margin: crash at 80 levels vs cap at 45 under ASan is under 2x; release builds have more
  room, but the margin is thin.

### Not run

- Non-ASan (release) build and the matcher thresholds in it; the thresholds above are ASan -O1.
- Remote/replication query construction (clean room).
- YAML cases after the first crash (block nesting at 20000, alias amplification); MessagePack
  SEARCH on maps; CPU-cost probes for wildcard expansion (`p3_expansion.py` still unfixed).
- No benchmarks, no TLS review, no differential run against the pre-S4 tree.

### Verdict after re-check

**S4 may not land yet.** SR1 is PARTIAL (nesting closed, phrase-width crash open: SR1b), SR2 is
CLOSED, and SR3 (MessagePack/YAML body nesting) is a crash reachable with a 1 KB unauthenticated
body. SR1b blocks. SR3 is pre-existing and outside the query path, so the lead may land S4 with SR3
tracked, but only if the "every case past a cap answers 400" wording is narrowed to JSON bodies.


## Re-check 2 (head `Xapiand:63de8e913`; fixes `Xapiand:c1fca3f6c`, `Xapiand:a38904396`, `Xapiand:5fd538f82`)

Clean room honored (no `src/xapian/`, `oracle.cc`, `sortable_serialise.{cc,h}` or
`src/server/remote_protocol_views.cc` opened). ASan+UBSan (-O1) build in its own dir
(`.scratch/s4-recheck2/build`, own deps, deleted after), probes in `.scratch/s4-recheck2/` (`chain.py`,
`nearnest.py`, `qstr.py`, `yaml.py`, `yaml_chain.py`, `restore.py`, `aggs.py`, `sr3.py` copied from
`s4-sr1b`; outputs `chain{A,B,C}.out`, `nearnest.out`, `qstr.out`, `yaml.out`, `ychain.out`,
`restore.out`, `aggs2.out`, `sr3.out`, ASan logs `node-*.log`). The probe harness kills any node whose
RSS passes 3 GB. Release build not run. Nothing outside this file was edited; no commit.

### Verdicts

- **SR1b: CLOSED.** The chain cap is not bypassable by anything I could construct.
- **SR3: PARTIAL.** JSON, MessagePack (buffered and streamed) and NDJSON are guarded and the two YAML
  crashes are fixed, but the YAML depth guard counts parse-time nesting, not alias-expanded nesting, so
  a 12 KB YAML body still crashes the node (SR3b), and a 340-byte YAML body exhausts memory (SR3c).
  Both are new findings; the changelog line "a JSON, YAML or MessagePack body 200 levels" is false for
  YAML with aliases.

### SR1b: CLOSED

Fix: `chain` per node in `set_extent` (`src/engine/query.cc:77-91`): a `combine` PHRASE or NEAR is 1,
AND and FILTER (also as `append`) add their operands' chains, every other operator takes the maximum,
refused past `MAX_QUERY_CHAIN` 256 (`query.h:98-100`, message "Query joins too many phrases").

- Tests and revert: `xapiand_test_engine_query` 22442 checks and `xapiand_test_text` 13224 pass under
  ASan+UBSan. With the `links > MAX_QUERY_CHAIN` throw disabled, 9 checks fail
  (`tests/test_engine_query.cc:989,990,999,1021,1025,1033,1038`); restored.
- Bisect to the largest accepted n and the first refused, then 2x and 4000, node restarted on death,
  42 shapes, **no crash anywhere** (`chainA/B/C.out`). Every AND-like shape answers 200 up to 256 chain
  units and 400 from 257 (n is phrases, or groups x phrases):
  - Text `_and` of phrases, boolean `AND`, groups of 2 and of 8 phrases ANDed (257 and 263 phrases: the
    sum is over the groups), phrases of 15 words (255) and of 50 words (80, the clause cap fires first).
  - DSL `_and` of `_phrase` and of `_near` (256, 15-word `_near` 255), `_filter` of phrases (256),
    `_and` of `_filter` pairs (257), `_and` of `_and` groups of 8 (263), text `_filter` of text groups
    (257), `_and` over sibling fields (256), a DSL `_and` whose children are text `_and` groups (259).
  - Phrases under OR, AND_NOT, XOR, AND_MAYBE (`+"a b" "c d"` and in groups), SYNONYM (`~`) and boost
    (`^2`) inside an AND, text and DSL: 256 groups, no crash. An OR of 8 phrases inside an AND: 655
    phrases fine (one chain unit per group, as designed).
  - `NEAR` between phrases and between OR groups in text: 1024 and 512 operands, no crash.
  - Flat n-ary `_and_not`, `_xor`, `_and_maybe`, `_elite_set`, `_or` of 1365 phrases (the clause cap):
    200 and alive. They take the maximum (chain 1) and the matcher survives them at full width.
- Nesting around the reset at PHRASE/NEAR (`links = 1` ignores their operands): `nearnest.out` nests
  AND-groups of phrases inside `( ... ) NEAR z`, up to 1,000 phrases over 8 to 40 levels: 200 up to 200
  phrases, then 400 "joins too many phrases"; no bypass. The reset is not reachable anyway: the text
  parser refuses a NEAR or PHRASE over non-words (`tests/test_text.cc:203-208`, `:292-298`) and the
  DSL's `_near` and `_phrase` objects are leaves (`query_dsl.cc:334-337`, `is_leaf = true`), so no
  PHRASE or NEAR node has an operator operand today. A future constructor that builds one would reopen
  this, so the factory could sum the operands' chains instead of resetting (one-line hardening).
- `q=`: AND, default-op, `+required`, `body:` and OR-group chains of `"a b"` at n = 255..1000
  (`qstr.out`): 200 and alive. The `q=` quoted text did not become PHRASE nodes in that request (1000
  ANDed `body:"a b"` answers 200, not 400), so the chain cap is not exercised on that path and there is
  nothing to bypass; the AND chain of 1000 terms costs 2.5 s CPU there (the known `make_dsl_query` cost).
- Caveat on margin: the cap (256) against the unmodified crash at 601 under ASan -O1 is 2.3x, and the
  probes at the cap were run at exactly 256 chain units (255-257 above); every shape that undercounts
  per-unit cost (phrases of 15 words, `_near` of 15 words, 8-phrase OR groups) was tried at the cap.

### SR3: PARTIAL

Guarded, with evidence:

- JSON: `json_load` (`src/database/utils.cc:174-`, `json_depth` before rapidjson) used by every JSON
  path, including form-urlencoded (`search_views.cc:2877-2886`, falls back to a string when refused) and
  NDJSON, both the buffered `decode` (`:2850-2858`) and the streamed RESTORE (`:2936-2975`, an
  over-deep line is dropped with `L_EXC`, `processed:0`).
- MessagePack buffered: `msgpack_load` (`utils.cc:241-249`) unpacks with `unpack_limit` depth 200 and
  throws `ClientError` before the recursive `MsgPack(object)` copy; `Request::decode` and the access-log
  prettifier (`search_views.cc:2873`, `:3217`) both use it. Streamed (`STREAM_MSGPACK`): the `unpacker`
  member is built by `make_msgpack_unpacker()` with the same limit (`search_views.h:158`).
- `text/plain`, `application/octet-stream` and any other type fall to `MsgPack(body)`, a string, never
  parsed (`search_views.cc:2888`).
- `sr3.py` against my build: 64 ok, 0 FAIL (MessagePack array and map nests 200 to 100000 on PUT and
  SEARCH, YAML flow and block nests 150 to 100000, field paths 100-199 answer 400, NDJSON POST). My
  `restore.py`: RESTORE of a database and of a document with MessagePack nested 150 to 200000, NDJSON
  lines nested 150 to 200000, text/plain, form-urlencoded and octet-stream bodies: node alive in every
  case, 400 or dropped objects past 200 levels.
- Revert: with `unpack_limit` depth lifted, `xapiand_test_text` and `xapiand_fuzz_body` die with an ASan
  stack overflow in `msgpack::object::object(const object&, zone&)`; with the YAML `++depth > maxDepth`
  check disabled, `tests/test_text.cc:485` fails on every depth case; restored (`git status` clean).
- `xapiand_fuzz_body 40 7`: 46,750 bodies, 12,007 decoded, 34,743 refused, no violation. It covers
  nesting and byte mutations; it has no alias generator, which is why it misses SR3b and SR3c.
- The two YAML crashes (empty document, bare scalar): `fuzz_body` and `yaml.py` show 400, no crash.
- 150-level field path: `sr3.py` PUT n=100..199 all 400 (`http_utils.h:101-107`, `errc::invalid` to 400).

### SR3b (critical, blocking for the claim "every body decoder is depth-guarded"; pre-existing): YAML alias chains expand past the depth limit and crash the node

`yaml_load`'s guard counts open collections as events arrive (`src/y2j/y2j.cc:189-201`). Aliases are
applied afterwards by `reference.Set(document, *value)` (`:490-500`), a deep copy, so a chain of
anchors that each wrap the previous one is 2 levels deep to the guard and N levels deep afterwards:

```
- &a0 [x]
- &a1 [*a0]
- &a2 [*a1]      (n lines, 16 bytes each)
```

`ychain.out` (fresh node each): n = 300, 400, 500, 600 answer 400 (refused later, by the indexed-object
check); **n = 800 (12.6 KB) and 1000 (15.8 KB) kill the node** on PUT and on SEARCH. ASan
(`node-ychain800.log`): `stack-overflow` in `msgpack::object::object(const object&, zone&)`
(`object.hpp:570`), 250+ frames, from `MsgPack(rdoc)` copying the expanded document. n = 5000 to 100000
(88 KB to 2 MB) also kill it and take 0.8 to 1 GB before dying. Unauthenticated, one request.

Fix direction: bound the expansion, not just the parse. Track each anchor's depth and node count when its
collection closes; at an alias, refuse if `depth + anchor depth` passes `maxDepth` or the running total of
expanded nodes passes a budget. Or refuse aliases in request bodies (they are rare in an API payload); that
is the smallest change and also closes SR3c.

### SR3c (critical, blocking for the same claim; pre-existing): YAML alias amplification ("billion laughs") is a memory and CPU bomb, a crash by OOM

Probe `yaml.py`: level 0 is a mapping of nine strings, level i is nine aliases of level i-1 (the classic
shape, 9^i leaves), top-level keys `a0..a(n-1)`:

| levels | body | PUT | peak RSS |
|---|---|---|---|
| 3 | 156 B | 204, 0.08 s | 156 MB |
| 4 | 202 B | 204, 0.23 s | 389 MB |
| 5 | 248 B | 204, 1.2 s | 476 MB |
| 6 | 294 B | 204, 11.6 s | 1.07 GB |
| 7 | 340 B | **no answer in 47 s; stopped by my 3 GB RSS watchdog** | >3 GB |

Each level is about 10x the time and memory. The bodies are accepted as documents (204), so the
expansion is not even rejected downstream. SEARCH with the same bodies is also expanded (level 7: 2.2 s,
964 MB RSS). A level-9 body (about 430 bytes) means 387 million nodes; the node dies of memory or
`std::bad_alloc`, and every request that survives pegs a worker. This is a crash/DoS class: blocking for
any listener open to untrusted clients. The fix is the same expansion budget as SR3b (a total-node cap
relative to the body size, or no aliases).
Also seen, not blocking: `a: &a [*a]` and `a: &a {b: *a}` are accepted (204) and the node stays up; an
alias used as a map key answers 500 (`MAP keys must ...`) rather than 400.

### Nested aggregation that did not answer within 120 s: a resource-exhaustion vector, separate from S4

`aggs.py`, `aggs2.out`: 300 documents, field `n` with 7 distinct integer values, a `_terms` aggregation
nested `k` times on `n` (the same payload as the original probe, 200 bytes):

| nesting k | time | peak RSS |
|---|---|---|
| 0 | 0.01 s | 285 MB |
| 1 | 0.04 s | 285 MB |
| 2 | 0.27 s | 341 MB |
| 3 | 1.9 s | 616 MB |
| 4 | 12.7 s | 945 MB |
| 5 | no answer in 60 s, node at 100% CPU, 1.15 GB and rising | |

Cost multiplies by about 7 (the number of distinct values) per level, so it is exponential in the nesting
and bounded only by the 200-level JSON cap (about 95 levels of `_aggs`); the 1-document, field-absent
payload of re-check 1 answers in 0.00 s at 90 levels, and `_values` nested 90 levels answers in 0.1 s
(linear), so the hang is specific to populated `_terms` and my earlier non-answer was this, not a payload
mistake. It runs on the worker with no timeout and no cap, from a 200-byte request, and repeats until the
pool is full. Likely a bug in how `TermsAggregation` fans out per bucket (the first-level `_doc_count`
of 342 over 300 documents shows documents are counted more than once), not the query path; the aggregation
code is untouched by S4 (`src/aggregations/` differs from the merge base only by the earlier callback
refactor; I did not run master to confirm it is pre-existing). It is not an S4 blocker (aggregations are
not on the text query path and S4 makes no claim about them), but it is a **separate high-severity
follow-up** (call it SR4): cap aggregation nesting (say 5 levels), cap total buckets while collecting
(as `_terms` and `_values` already cap their own buckets for the engine callbacks), and find why the
fan-out multiplies.

### Other items

- Changelog (`docs/src/content/docs/changelog.md:14-16,24-27`) states YAML bodies are limited to 200
  levels and that a "YAML flow sequence 1,000 levels deep" answers 400. True for flow nesting, false for
  aliases (SR3b, SR3c). Narrow it or fix the aliases before landing.
- The 2.3x chain-cap margin is measured under ASan -O1 only; release has more stack, so its margin is larger but unmeasured.

### Not run

- Release (non-ASan) build and its matcher thresholds; benchmarks (none run).
- `src/server/remote_protocol_views.cc` (clean room); replication query construction.
- The aggregation hang on master (to confirm pre-existing); the cause inside `TermsAggregation`.
- A level-8 and level-9 billion-laughs body (stopped at 7 by the 3 GB RSS watchdog on a shared host).
- HTTP chunked-transfer variants of the body decoders; TLS.

### Verdict after re-check 2

SR1b is CLOSED, and with it the text query path: it is bounded in depth, clause count and
matcher-recursion width, with 42 shapes surviving at the cap. SR3 is PARTIAL: the decoders named in the
request are guarded, but YAML aliases bypass the depth guard and crash the node with a 12 KB body (SR3b)
and exhaust memory with a 340-byte body (SR3c). Both are pre-existing, outside the text query path and
cheap to fix in `y2j.cc` (an expansion budget, or no aliases).

**S4 may land as the text query path.** It may not land with the changelog claim "a JSON, YAML or
MessagePack body 200 levels" and "no longer crashes the node" unless SR3b and SR3c are fixed first
(recommended: small, and it makes the claim true) or the wording is narrowed to YAML without aliases and
SR3b/SR3c are tracked as blocking for any release exposed to untrusted clients. The nested-aggregation
blowup (SR4) is a separate follow-up and does not gate S4.
