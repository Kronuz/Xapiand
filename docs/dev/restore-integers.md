# Integer typing: by value, not by encoding

Status: on `quest/xapiand/restore-mixed-sign-integers` (check `tests/restore_cli_check.sh`).

## Symptom

x7-coverage's probe (job 261111-162310-eb4bbc) restored `tests/corpora/source.ndjson`, 3,000 documents, over
ndjson RESTORE: 3,000 indexed. `--dump` wrote 3,000 entries. A msgpack RESTORE of that dump answered 200 with
`"indexed":1498`; the other 1,502 items were 500 "Negative value of NEGATIVE_INTEGER is not a valid
POSITIVE_INTEGER". The corpus's `big` field holds both signs, and its first document's value is positive.

## Cause

A dynamic field takes its type from its first value (`Schema::guess_concrete_type`, `src/database/schema.cc`). For
integers it used the msgpack type: POSITIVE_INTEGER gave `positive` and NEGATIVE_INTEGER gave `integer`. That type
reflects the encoding, not the value:

1. A msgpack packer writes every non-negative integer unsigned, so a msgpack body's `5` is POSITIVE_INTEGER.
2. The JSON reader (the rapidjson adaptor in Kronuz/msgpack `xchange/rapidjson.hpp`) writes integers that fit
   int32 or int64 signed (NEGATIVE_INTEGER, even when non-negative), and those in [2^31, 2^32) or at least 2^63
   unsigned.

So the JSON source typed `big` (first value 540948982818) `integer`, and its dump, restored as msgpack, typed the
same field `positive`. Each later negative value then reached `Schema::index_item`'s `positive` branch, which
called `MsgPack::u64()`. That accessor rightly throws msgpack's `type_error` for a negative value, and the request
became a 500. JSON input had the same failure for a field whose first value is in [2^31, 2^32).

The msgpack library behaves correctly here. The bug was in how Xapiand typed integers.

## Fix

The value now picks the type, whatever its encoding:

- `Serialise::guess_type` and `Serialise::guess_serialise` (`src/serialise.cc`, also used for `_id`) give
  `integer` to any integer that fits int64 and `positive` only beyond it. Strings already followed this rule
  (try int64, then uint64).
- `guess_concrete_type` uses that rule for both msgpack integer types. A field that already has an integer type
  takes a value when the value fits that type's range.
- A value outside an integer field's range (a negative value into `positive`, one at least 2^63 into `integer`,
  a float into either) answers 400 "Format invalid for ... type" instead of msgpack's 500.

A field whose first msgpack integer is non-negative now infers `integer`, not `positive`. A later value of at least
2^63 in that field is refused, which JSON input already did. The full uint64 range stays available by declaring
the field `positive`. Fields already typed in an index's schema keep their type.

## Evidence

- `tests/restore_cli_check.sh` generates documents whose `n` and `w` fields are positive for the first half and
  negative after it (`w` beyond 2^31). It restores them over ndjson, dumps them with `--dump`, restores the dump
  over HTTP msgpack and with CLI `--restore` (default shards, `--shards 1`, `--shards 3`, and into an index made
  by PUT), and requires every RESTORE to index all N documents and every DUMP to equal the source documents.
  It passes at the fix (job 261111-171323-80d18b) and fails with master's `src` (job 261111-171345-4024c7).
- With `CORPUS=tests/corpora/source.ndjson` (x7-coverage's corpus), the same check round-trips all 3,000
  documents, comparing them one by one (job 261111-171323-80d18b).
