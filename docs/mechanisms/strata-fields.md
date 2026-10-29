> Status: CURRENT (2026-10-28) describes `phase7a/strata-fields` (SF); not landed.

# Strata fields and keys: how it works

The strata engine writes and reads a term as a field and some bytes, the way the engine seam names it, and keeps a
document's key apart from its terms. The Xapian engine still spells the same seam as flat terms. This is SF of strata's
`docs/design/fields-and-keys.md` (D2, D6, D7, D8, D12, amendment A-X6); the seam is in `seam-fields.md`.

## How it works

```
Schema / QueryDSL          seam                              strata engine                         prospect
 field_name + term  --->  Document::add_term(field, term)    DocField{kind, terms}   --->  field_part(declared(i) | id(n))
 id field's terms   --->  KEY_FIELD, type char + id bytes    key + key_wdf (no term) --->  document key, length += key_wdf
 Query::term(f, t)  --->  QueryNode{field, str}              term_table(field, bytes) --->  find_term(snapshot, field, bytes)
```

1. **Writing.** A `StrataDocument` holds its terms by field name, with the widest kind the document asked for (`boolean`,
   `terms`, `text`). `fill()` (`shard.cc`) names each field in the transaction: a field the shard's table already has is
   `field_ref::id(n)` under `expected_fields = table.mark()`; a new one is declared by the transaction (`declared(i)`) with the
   postings its kind says (docs, freqs, positions); the key field `_id` is declared once, with role `key`. The table keeps the
   declaration of the first document that named a field; later documents reference its id, so a different kind never conflicts.
2. **The key.** `Document::set_key(key)` names the document; the key is the id's type character and its encoded bytes
   (`database/utils.h` `document_key`). Terms the host adds under `engine::KEY_FIELD` are the key's: they hold no postings, and
   their frequencies add to the document's length (the Xapian id term's frequency). The host passes key bytes to
   `replace_by_key`, `erase_by_key`, `find_key` and `WriteResult.term` unchanged. Multi-shard placement hashes `"Q" + key`
   (`key_placement_hash`), the spelling the Xapian engine always had, so an index keeps its documents where they are.
3. **Reading.** A query node names its field. The shard set resolves the name to each segment's id through the field table
   its pin carries (`Pinned::fields()`); a shard that does not know a name answers no terms. A term on `KEY_FIELD` is a key
   leaf. `expand()` (`expand.cc`) scans one field's dictionary (or the keys for the key field) per wildcard or edit-distance node;
   a wildcard with an empty pattern (`Query::field`) scans every field whose name starts with the node's, as the Xapian engine
   reads the name as a prefix. A phrase over two fields matches nothing.
4. **The Xapian engine** joins field and term exactly as before (`xapian/legacy_terms.h`), and writes the key field's terms with a
   leading `Q`: its terms, answers and log lines are byte-identical to the flat spelling.
5. **The log.** A strata document's WAL line is format 3: the version, the key, the key frequency, the data, the values, then
   the fields with their kinds and terms. Replay re-declares by name, so it never depends on an id a lost commit assigned.

## Keys

The index is opened with `index_options::keyed` and holds the host's key bytes as given; an index is keyed only if it was created keyed.

## Invariants

1. A term is found under the field it was written to and no other (`test_fields` in `test_engine_search_strata`: the same word in two fields).
2. A key is never a term of another field and a term is never a key (`test_fields`, `strata_only`: terms that look like keys; keys that look like terms).
3. The table keeps the first declaration of a field (`declared` in `test_engine_strata`: kinds, the key field once, reopen, replay).
4. The Xapian engine's terms are the flat spelling (`test_key_field`, `test_xapian_terms_are_joined` in `test_engine_fields`).
5. A document line round trips and damaged input is a syntax error (`codec`, `fuzz_strata_document`).

## Code

`src/engine/strata/{document,shard,search,expand,keys,pin}.*`, `src/engine/xapian/legacy_terms.h`,
`src/database/{utils,schema,handler,shard}.cc`, `src/query_dsl.cc`. Ledger: `docs/status/strata-fields.md`.
