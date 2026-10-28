> Status: CURRENT (2026-10-28) code review of `phase7a/seam-fields` (X6); nothing supersedes it.

# Code review: seam-fields (X6)

Branch `phase7a/seam-fields`, Xapiand:a919deb6f, Xapiand:65a3df348, Xapiand:7f4e3a71f, from master `5d7f3263d`.
Design: strata `docs/design/fields-and-keys.md` section 7 (X6), D12, C3, C4.

## Verdict: APPROVE WITH CONDITIONS

No blockers on the Xapian engine. The seam is joined in one place per engine and the Xapian terms are byte for byte the
old ones. Two conditions concern the rebase onto the S2 search code now on master; neither needs a fix round on this branch.

## Evidence

Build: ASan+UBSan (Apple clang, RelWithDebInfo -O1 plus the repo's -O2 -DNDEBUG, own `_deps`), own dir
`.scratch/x6-review/build` (deleted). `engine_fields` 1237, `engine_document` 645, `engine_query` 22395, `engine_search`
4423, `engine_strata` 9559 checks pass; no ASan or UBSan report in the tests. Matches the ledger.

Probes (all in `.scratch/x6-review/`): `build.sh` (the sanitizer build), `probe_build.sh` (compiles a probe with the
fields test's flags and link line), `probe_fuzzy.cc`, `probe_refs.cc`.

## Probe 1: every term path goes through (field, term) - clean

`git grep` outside `src/xapian` and the Xapian engine:

1. `prefixed(` has no hit anywhere (X2 holds). The remaining `flat_term(term, prefix, ctype)` callers are the document key
   (`handler.cc:1305`, `schema.cc:2791-2911`, used for the id term and `fnv1ah64` shard choice, as before) and log lines in
   `generate_terms.cc:165-178`.
2. The strata engine joins in `interim_terms.h` only (`strata/document.cc:140,162`); `set_key` stores the key string
   untouched. C3 grep `key_lead|engine_key|term_key_prefix|key_prefix` in `src/engine/strata`: 9 lines, as on S1.
3. The Xapian join is `convert.cc` `join()` and `query.cc` `joined()`; the `V` reader at `xapian/shard.cc:80` is inside the
   Xapian engine and agrees with the writer at `shard.cc:63`.
4. Two harmless inline joins in the host: `query_dsl.cc` `config.field = prefix + ctype` (equals `field_name`) and the
   `"Z" + prefix` stem field in `generate_terms.cc` / `xapian/text.cc`. Both are the documented D12 spellings.

## Probe 2: fuzzy census change - same documents

`probe_fuzzy.cc`: in-memory Xapian database, 6 field names (including `Q`, `a`, the empty name, an accuracy name
`<186a0>.N`) times 11 words, one document per pair; 792 queries (every field, word, edit distance 0 to 2, prefix length
0, 1, 2, 5), each run through `to_xapian(Query::edit_distance(field, word, ..., prefix_length))` and through the hand-built
master form (`OP_EDIT_DISTANCE` on `field + word` with `prefix_length = field.size() + pl`). 792 of 792 return non-empty
and identical document lists; 0 differ. The only production caller passes `fuzzy_prefix_length = 0`, which master encoded as
`prefix.size() + 1 + 0` and the branch as `field.size() + 0`, the same number (`query_dsl.cc:735-736` in the diff). The
census counter loses its single sample only because the engine-visible argument changed meaning, as the ledger says.

## Probe 3: the open items - none is a Xapian-engine gap today

1. `DocView::terms(prefix, sink)` (`aggregations/metrics.cc:125`) and `term_details` hand out whole Xapian terms: that is
   master's behaviour on the Xapian engine; the strata `DocView` already answers `unavailable()`. SF work.
2. Opaque keys: `set_key(key)` adds the same boolean term as master; the id term is built by the host as before. SF work.
3. `field: "*"` over an object: the Xapian engine reads the name as a prefix, as master did (`Query::field` maps to the
   same `OP_WILDCARD` description, tested). On the strata engine the name would match exactly, so it silently narrows; SF
   work (design D12 says SF builds the OR from the schema).
4. The `Z` stem marker: `term.front() == 'Z'` under `all_z`/`some` only. The analysis library lowercases words, so no word
   term can lead with `Z`; `test_text_fields` checks the document against the generator run with the prefix across
   prefixes `Zname.S` and `ZZ.S`, four stem strategies, three languages, texts with `Zzz ZZ Z z zealous`, and
   `test_word_break_fields` does the same for the ICU word-break generator. No gap. It is a convention rather than the
   design's additive `analysis` change; if the pinned library ever emits an upper-case `Z` word term (a new token class), the
   Xapian engine would file it under the stem field, silently. See CR2.

## Probe 4: FieldRef lifetime - cannot dangle in contract use

`probe_refs.cc 0`, ASan+UBSan, both engines: 5000 refs kept across `Document` move construction and move assignment onto a
live document (`field()` returns the same ids, string_view keys in `field_ids_` stay valid because the deque keeps its
nodes); a ref held while 100000 more fields are declared; kind widening; serialise then unserialise. Clean.

Out of contract (ref used on the wrong document): `probe_refs.cc 1` (a ref from a 5000-field document on a document with
0 fields) and `probe_refs.cc 2` (default `FieldRef{}` on an empty document) fault in `deque::operator[]` (null base,
SEGV). A foreign ref on a document that has enough fields would not fault but would silently name another field. No
production code stores a ref across documents (grep: refs are locals in `generate_terms.cc` and `schema.cc`). See CR3.

## Findings

- CR1 (condition, rebase): master now holds S2's `src/engine/strata/search.cc`, which reads `node->str` as the whole flat
  term (`search.cc:317-324`) and treats an empty `str` as match-all. After this branch lands there, a marker term (field set,
  empty term) is read as match-all, and every field-qualified term is looked up without its field, so no field-qualified
  query finds anything. The rebase must route `term` through `interim::flat_term(node->field, node->str)`, make match-all
  `field.empty() && str.empty()`, and add the field length to the edit-distance prefix length wherever strata supports it
  (it rejects wildcards and edit distances today). Add a strata search test with two fields holding the same word, and one
  with a marker term. The C3 count after the rebase must not rise above the sum of the two branches' counts.
- CR2 (non-blocking, architect): stem-field detection by the leading `Z` is a host-side convention that two code copies
  repeat (`xapian/text.cc:97-110`, `generate_terms.cc:553-570`). The design's `analysis` change (report the stem flag)
  removes both; keep it on the SF list and do not add a third copy.
- CR3 (non-blocking): `Document::add_term(FieldRef, ...)` indexes the deque unchecked (`document.h:105-122`). A debug
  `assert(field.id < fields_.size())` would turn the out-of-contract use into a clear failure; ids carry no document tag, so
  a foreign ref that happens to be in range misattributes silently.
- CR4 (non-blocking, architect): `Field::kind` and its widening have no consumer on either engine, and the only checks are
  the id-stability test and a random declaration loop. The kinds Schema chooses (`schema.cc:5309`: positions mean `text`,
  `bool_term` means `boolean`, else `terms`; accuracy and geo terms default to `terms`) are not verified against D12 until
  strata reads them. SF should add the consumer and a test that fails on a wrong kind.
- CR5 (non-blocking, architect): the design shows `FieldKind::key` and `set_key(std::string)` with the Xapian engine adding
  the `Q` term; the branch keeps `set_key(key)` with the host building the term (ledger open item 2). Accepted as a
  deliberate narrowing; record it as a design amendment, since SF's shard key path depends on it.
- CR6 (non-blocking): `index_partial_paths` (`schema.cc:5282`) declares the whole partial-path term as a field name with an
  empty term. Byte-identical on Xapian; on strata after SF it makes one field per path. Same class as D12's "partial paths
  are fields of their own", but their types should be checked then.

Scope: nothing beyond the design except the `V` term as field `V` (design says it has no strata field; here it exists on
both engines through the interim join, as on master). Tests: the oracle tests compare against the generator and hand-joined
Xapian objects, so they fail without the join; the ledger's ten mutants are plausible against them (I did not rerun them).

## Conditions

1. CR1 is addressed by whoever rebases S2/S3/S4 onto this branch, with the strata search tests named above, before the
   merge to master.
2. CR2 to CR6 go to the SF ledger; none needs another review.

## Not run

Docs E2E, SEARCH dumps, `:info` and term-dump identity (`harness/fields_identity.py`), census, mutants, `terms_diff.py`:
the ledger's numbers were not reproduced. No master baseline build existed, so probe 2 compares with the master formula
by construction, not with a master binary. TSan (no shared state added), CI, other platforms, the cluster harnesses, a
performance check of the per-term `doc.field(...)` hash and `field_name` string in the indexing path (not measured).
