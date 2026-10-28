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
