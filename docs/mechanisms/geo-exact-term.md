> Status: CURRENT (2026-11-18) how a geo field's exact-match term is made; quest serialize-validation.

# Geo exact-match term

A geo field indexes its whole geometry (a point, a circle, a polygon, a collection) as one exact-match term, so a query
for that same geometry finds the documents that hold it. The term is `Serialize::ranges_hash` (`src/serialize.cc`) of the
geometry's HTM trixel ranges: the first 16 bytes of SHA-256 over each range's fixed 14-byte encoding
(`Serialize::range`, start and end trixel ids), in order.

## Collision bound

Sixteen bytes are a 128-bit hash. n distinct range lists collide with probability at most n(n-1)/2^129: about
1.5 x 10^-21 at 10^9 distinct geometries and 1.5 x 10^-15 at 10^12. A 64-bit hash would collide with probability about
2.7% at 10^9. SHA-256 is the hash in the tree whose bound holds for any input, adversarial or not; the encoding is
fixed-width, so two lists hash alike only by collision.

The term used to be `std::hash` of the range list, which xored each range's start with its end. A point's range has start
equal to end, so every point hashed to 0 and indexed the same term: an exact point query matched every document with
a point (the docs example answered 10 accounts, the page size, instead of the 3 at that point).

## Cost

437 ns per one-range term (a point), 968 ns for 8 ranges, 7.2 us for 64, 55 us for 512: about 107 ns per range past
the first 64-byte block (`quiet strict` beside two light holds, i9-9980HK, the portable SHA-256 kernel since this CPU
has no SHA-NI; harness `.scratch/xapiand/serialize-validation/hash_bench.cc`). The xor it replaces cost about 1 ns per
range. The term is made once per geo value at index time and once per exact geo query; it has not yet been set against
the rest of a geo value's indexing (range generation and accuracy terms).

## Who reads these bytes

- The indexer: `Schema::index_item` for `FieldType::geo` writes the term under the field's prefix and the global one
  (ctype `G`), using the field's `_partials` and `_error` for the ranges.
- The exact-match query: `QueryDSL::get_regular_query` serializes the queried geometry through `Serialize::MsgPack`
  (`object` or `string`, then `geospatial`), and `get_global_query` through `guess_serialize`; both end in
  `ranges_hash`.

Not affected: sorting (the value slot's centroids, read by the keymaker), aggregations (the value slot's ranges), and
geo range and `_in` queries (accuracy trixel terms and the value slot). `Serialize::ranges_centroids` and
`GenerateTerms::geo` write those, and this term changes neither.

## Existing indexes

The new 16-byte term never equals an old term (a 1 to 10 byte length-prefixed hash), so an index written before this
change gives no false match: an exact geo query on it finds none of its old documents. Reindexing restores exact
matches. Sorting, aggregations and `_in` queries on an old index answer as before.
