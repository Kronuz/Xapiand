# exact-id-keys review: correctness

Commit: Xapiand:1d073c52f (code identical to Xapiand:68f26a25e, where the gate jobs ran; the two later commits touch the journal only)
Verdict: APPROVE
Date: 2026-11-15

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261115-210614-d6a201 at 68f26a25e: exit 0, 40/40 ctest (A4, local half). CI at 1d073c52f is green per the lead.
- A1 test 261115-210629-01840c: exit 0, 743 checks. Mutant old-codec 261115-210632-ca9170: exit 0, KILLED with 147
  failures. The failures are end-to-end, not only the key unit checks: GET `n`/`_id` (test_node.cc:2468-2469), `_id`
  term-search hits (2472), totals (2474, 2518), and the pair partner after a DELETE (2515).
- A2 test 261115-210635-274f76: exit 0, 95 checks. Mutant cap-2-32 261115-210639-ed1c27: exit 0, KILLED with 45
  failures: POST success (2237), ids above 2^32-1 (2581), the D85 bucket holder (2583), and versions/values after the
  reopen (2568-2569).
- Both tests check values: each id's own `n` and `_id`, the exact hit list, version 1 (no overwrite), the bucket
  holder per issued id, and key order byte by byte. Risk 4 is answered.

## What I checked myself

Read the quest diff (src, CMakeLists.txt, tests, docs) and traced every id-key site:

- Writes: `Schema::index` (schema.cc:2881, 2901) uses `guess_id` or `Serialise::id`. The `_id` field's own
  `index_term` still builds an old-codec key term (schema.cc:5305, via `Serialise::integer` at 5383), but
  `StrataDocument::add_term`/`add_posting` on `KEY_FIELD` keep only the wdf (engine/strata/document.cc:379, 404).
  The key is `set_key(term_id)` alone (schema.cc:2984). No second key reaches disk.
- Lookups: GET, DELETE, PATCH, UPDATE, UPSERT and RESTORE placement all go through `get_prefixed_term_id`
  (handler.cc:1630, 1639).
- Automatic ids: `issue_id` uses `numeric_key` (shard.cc:417). `numeric_id` (handler.cc:140) uses strict
  `sortable::decode`. `next_auto_id` stays bounded by `MAX_NUMERIC_ID` and cannot overflow (2^63 - 1 + 1024 fits a
  uint64). The floor goes through `serialise_length(unsigned long long)`, so nothing truncates to 32 bits.
  `stored_numeric_id` (replay) agrees with `numeric_id` at the 2^63 and float bounds.
- Responses: `unserialise_term_id` 'N' (handler.cc:2260) decodes only freshly written keys (POST, bulk, search_views).
- Term search: `get_regular_query` (query_dsl.cc:685-686). Ranges and sorting on `_id` stay on the value slot and
  accuracy terms, as the quest scopes them.
- Replication/WAL: strata's log carries each document's key bytes, and no path re-derives a key from an id. DUMP
  (`_dump_document`, handler.cc:942) reads stored data, not keys, so an old index can still be dumped for the
  documented RESTORE.
- `NEW_NUMERIC_KEY` is "N"; no real key equals it, because `encode` is never empty (the suite's PUT of id 0 is its own
  document).
- Pinned Kronuz/sortable ac13b77: the later commits to its HEAD only add trusted decoders to sortable.hh. The encoder
  and strict decoder are unchanged.

Boundaries (risk 2): 2^32-1, 2^32, 2^53±1, 2^63, 2^64-1 and the cap are under test. 2^63 - 1 as an explicit id in an
integer index lost its round trip (N3). Negatives and fractions route by hash over the new bytes, consistently on
write and lookup. A float placeholder refuses issued ids above 2^53 before the counter moves.

Existing indexes (risk 3): there is no detection. An index written before keeps old-codec keys, so GET/DELETE by id
silently miss and a PUT adds a second document. This is stated in the design note's status line and in the changelog
("needs a dump and a RESTORE"), and it is consistent with D78/one-id. It is acceptable under "formats open until
release" (N2).

Probe job 261115-215450-71a3de (`.scratch/review-exact-id-keys-correctness/probe_query_casts.sh`) was still queued
behind an exclusive benchmark chain when I wrote this. It checks N1's three cases plus 4294967297.0 and `q=_id:N`. It
exits 0 only when every off-type search returns no hits with 200.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | defect | A term search on a numeric `_id` now casts its value with `Cast::cast` instead of serializing it strictly. `{"_id":1.5}` in an integer index now matches document 1 (`Cast::integer` truncates, cast.cc:163). `{"_id":-1}` in a positive index matches 2^64-1 (`Cast::positive` wraps, cast.cc:185). `{"_id":"abc"}` throws CastError, a 400, where master's SerialisationError fell back to the global query (200, no hits; query_dsl.cc:482-485 catches only SerialisationError). Writes and lookups by JSON value already cast like this (pre-existing), so this aligns the search with them; only the query side is new. | query_dsl.cc:686, serialise.cc:934-952; probe 261115-215450-71a3de (queued) | Owner's call, since a fix narrows what matches. Options: (a) keep it; (b) in the query branch, throw SerialisationError when the value is not exactly a number of the field's type, so it falls back to no hits as on master. The probe is the test for (b). |
| N2 | no | gap | An index written before this change opens without error and silently misses its numeric ids, as documented. No version marker refuses it. | design note status line; changelog.md:179-182 | None needed under the ruling. A format-version check could come with format-freeze. |
| N3 | no | gap | `test_document_ids_are_themselves` replaced 2^63-1 with 2^62 (R4). An explicit int64-max id in an integer index (the untyped `guess_id` path) no longer has a PUT/GET round trip. The cap test uses a positive index, and A1's integer pairs stop at 2^53+1. | test_node.cc:1662 | Add a PUT/GET/term search of 9223372036854775807 (and -9223372036854775808) to the A1 integer index; no POST is needed. |
| N4 | no | nit | docs/design/one-id.md still states the 2^32-1 bound. The new note says it supersedes that bound, but the old note is not updated. | journal R5 | One line in one-id.md pointing to exact-id-keys.md. |
| N5 | no | nit | Equality of `encode(uint64 N)` and `encode(double N)`, which float-placeholder keys rely on, is tested at one value (2^40). It rests on the library's own cross-type guarantee. | test_node.cc (A1 key checks) | Optional: loop the A2 ids through `Serialise::id(floating, double(id))` for id ≤ 2^53. |

## Not run

- Probe 261115-215450-71a3de had not run when I wrote this (it was queued behind exclusive holds), so N1 rests on
  code reading. The lead can read the probe's log when it finishes.
- No probe at master for N1's baseline: master's behavior is read from serialise.cc:386-460 and query_dsl.cc:482-485.
- Lua `call_script` receives key bytes. I did not check whether any script decodes them.
- Sanitizers and other platforms: CI's.
- The `query-old-codec` and `index-commit` mutants in tests/exact_id_mutants.sh have no job I checked.
