# Aggregations over one shard and many

`tests/test_aggregation_shards.cc` indexes one corpus (240 documents: a multi-valued integer, a floating value
that is a multiple of 0.25, and a multi-valued keyword) into 1, 2 and 5 shards, three ways: no deletes, deletes,
and deletes that leave one of the five shards empty. It runs each aggregation request below over every layout and
compares the answer to the one-shard answer, bucket by bucket and in order, in three ways:

1. One spy over all the shards (`engine::search` over the shard set): what the server does.
2. One spy per shard, the partials merged from `serialise_results` bytes (the remote-shard path).
3. One spy per shard, the partials merged as objects.

Integer and string values are compared exactly; floating values to one part in 1e9 (the corpus makes sums exact, so
only variance and standard deviation can round). `test_one_shard_matches_the_rows` checks the one-shard answer against
the rows themselves, so the reference is not wrong in the same way as the layouts.

## Kinds covered

| Kind | Over | Notes |
|---|---|---|
| `_count`, `_sum`, `_avg`, `_min`, `_max`, `_variance`, `_std_deviation`, `_median`, `_mode`, `_stats`, `_extended_stats` | integer, floating | `_mode` ties go to the smallest value, so they are layout independent |
| `_values`, `_terms` (and the deprecated `_value`, `_term`) | integer, floating, keyword | `_limit` 1000: all buckets |
| `_histogram` | integer (interval, interval and shift), floating | ordered by key |
| `_range` | integer, floating | ordered by range position |
| `_filter` (single value, list) | keyword, integer | with metric and bucket sub-aggregations |
| nested | bucket > metrics, bucket > bucket > metric, filter > bucket > metric, several at once | |

Reserved but not served (they raise "reserved but not implemented yet"): `_cardinality`, `_geo_bounds`,
`_geo_centroid`, `_percentiles`, `_percentiles_rank`, `_scripted_metric`, `_date_histogram`, `_date_range`,
`_geo_distance`, `_geo_trixels`, `_missing`, `_ip_range`, `_geo_ip`.

## Differs by design

`_terms` and `_values` keep about `_shard_size` buckets while collecting and send the best `_shard_size` in a
partial. Past it a dropped value that returns restarts its count, so a count falls short of the true one, never
above it (the release-rows ruling). `test_terms_past_shard_size_are_approximate` indexes 5 hot values among 350
seen once, with `_limit` 5 and `_shard_size` 20, and checks on every layout and path that no count exceeds the
true one, that the top 3 keys are the hot ones in order, and that their counts are within 5%.

## Finding: merged partials lost their bucket order

A bucket orders by `slot` (its numeric key) and `idx` (its range position), set only while collecting
(`BucketAggregation::aggregate`). `serialise_results` did not send them and both `merge_results` overloads built
new buckets without them, so a merged bucket sorted by its key string (-15 before -17, 10 before 5) and ranges
in no order. Every `_values`, `_terms`, `_histogram` and `_range` over a numeric field, and everything nested in
one, differed between the merge paths and the one-spy path; metrics and keyword buckets did not.

The fix: `serialise_results` sends each bucket's `slot` (a long double as two doubles that add up to it) and
`idx`, and both `merge_results` overloads set them on a bucket they create.

The bug was latent. Nothing in the server merges partials: `DatabaseHandler::get_mset` gives the engine one
borrowed spy and the strata engine's spy sees every shard. Only the `MatchSpy` merge API (`serialise_results`,
`merge_results`) carried it, for a remote-shard or cross-node merge.

## Running it

`AGG_PARITY_DROP_SHARD=k` leaves shard k out of every n-shard run: the test must then fail. Reverting the fix
must make it fail too. `AGG_PARITY_ORDER_INSENSITIVE=1` compares buckets by key instead of position, to tell an
order difference from a value difference.
