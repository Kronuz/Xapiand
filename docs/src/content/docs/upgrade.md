---
title: "Upgrading"
---

This guide takes a node from **1.0.0-alpha.9**, **1.0.0-alpha.10** or **0.40.0** to **1.0.0**.
The short version: 1.0.0 searches with the [strata](https://github.com/Kronuz/strata)
engine, which does not read the indexes that those releases wrote with the Xapian engine, so your documents
move over with a [dump and a restore](/Xapiand/reference-guide/indexes/backups).
Xapiand runs as a single node; the cluster returns in a later release.

## Data compatibility

1.0.0 reads only indexes that the strata engine wrote. An index written by 0.40.0, 1.0.0-alpha.9 or
1.0.0-alpha.10 holds Xapian's on-disk format, which 1.0.0 cannot open. A node started on such a data
directory exits with status 65 and names the first Xapian file it found (`iamglass`, `flintlock`,
`postlist.*` or `*.glass`, in the registry or in an index's shard directory), and creates nothing. There is no
option to skip the check.

Carry the documents over instead: dump each index with the final Xapian-only release (the `xapiand`
branch) and restore it into 1.0.0. A dump holds every document's data with its `_id` and `_version`; the
restore indexes the documents again, so the new node builds every term, value and number encoding itself
(see [What changes](#what-changes-in-a-restored-index)). Index settings and explicit schemas are set
again on the new node, before the restore (see
[Restore using different schema](/Xapiand/reference-guide/indexes/backups#restore-using-different-schema)).

<a id="upgrading-from-100-alpha9"></a>

## Upgrading from 1.0.0-alpha.9 and 1.0.0-alpha.10

Both alphas searched with the Xapian engine. The steps are the same as for 0.40.0, and the old data
directory stays untouched until you are done.

1. Stop nothing yet: run the final Xapian-only release (the `xapiand` branch) against the old data
   directory, and dump every index. From the command line, with the same parameters you start the
   server with (at least `-D`):

   ```sh
   xapiand -D /var/lib/xapiand --dump="twitter/tweet" --out="twitter.msgpack"
   ```

   Over HTTP, `DUMP /twitter/tweet/` streams the same records
   (see [Backups](/Xapiand/reference-guide/indexes/backups)). Restore large indexes from the
   command line: a `RESTORE` over HTTP answers `413` for a body over 16 MiB, and for a chunked one.
2. Stop the old node and let it shut down cleanly.
3. Start the 1.0.0 binary on a **new, empty** data directory (`-D`). Do not point it at the old one.
4. Create each index with its settings and schema, if it had explicit ones.
5. Restore every dump into the new directory. From the command line, with the new server's parameters:

   ```sh
   xapiand -D /var/lib/xapiand-1.0 --restore="twitter/tweet" --in="twitter.msgpack"
   ```

   Over HTTP, `RESTORE /twitter/tweet/` takes the body of the dump.
6. Start the node and check each index: its document count against the old node's, and the searches
   you depend on (see the list below).

The command line `--dump` and `--restore` exit once the data is written.

## What changes in a restored index

The documents are the same; the engine around them is not. These are the changes a restored index shows,
grouped by what you meet first.

### Stored documents and numbers

- **Numbers compare exactly across types.** An integer, an unsigned integer and a floating field encode to
  the same bytes for the same number, so sorts, ranges and `_min`/`_max` agree with the true values,
  including integers above 2^53 (snowflake ids, nanosecond times, hashes), which the old encoding merged.
  A value that has no place in that order is refused instead of stored wrong: a NaN answers `400`, a
  `uint64` of 2^63 or more given to an integer field answers `400` (it wrapped negative), and `-0.0` indexes as `0`.
- **Dates read differently at the edges.** A new field whose first value is `2020-01-01T10:00:00.Z` (a fraction dot
  with no digits) or has a fraction of more than 62 digits is typed `text`; `2020-12-15||+12M` is `2021-12-15`.
- **A document that repeats a key keeps one entry**: `{"a":1,"a":2,"b":3,"c":4}` is stored as `{"a":2,"b":3,"c":4}`.
- **Geospatial ids of points near a cell edge can differ.** On builds that fuse multiply-adds (Apple silicon,
  `-mfma`), 0.7% of random points sit in the neighboring trixel; the restore recomputes them.
- **Ids.** See [Identifiers move to the v6 format](#red-flags).

### Schema and queries

- **`_cjk_words` is gone.** The property is unknown, and queries no longer split CJK words; use `_cjk_ngram`.
- **`_terms` over an analysed `text` field answers `400`**, and `_terms` counts a field's stored values: keyword
  buckets keep their case (`Red` and `red` are two buckets). `_terms` and `_values` keep about `_limit * 1.5 + 10`
  buckets while they collect (`_shard_size`), so counts can fall short when a field has more distinct values;
  over geo fields they answer `400`.
- **`_elite_set` answers `400`**, `nearest` and `fuzzy` (with `n_rset`, `n_eset`, `n_term`, `field`, `type`) are
  ignored and the search runs as an ordinary one, and `INFO` on a document no longer lists its terms.
- **Text query strings follow precedence and refuse malformed input.** `a AND b c` is `(a AND b) OR c`;
  `a NOT`, `a ()` and `(a` answer `400`. A lone `-a` in a field query answers `400`.
- **Some query shapes answer `400`**: a `_synonym` over a wildcard, a fuzzy word or a group; an array given to
  `_scale_weight`, `_wildcard` or `_edit_distance`; a `_phrase` or `_near` over anything but words.
- **Sorting by a string metric is corrected.** `Jaro`, `Jaro-Winkler`, `Sorensen-Dice` and weighted `Levenshtein`
  follow their standard definitions; sorting by `soundex` against `DICKSONX` gives `DICKSONX`, `DIXON`, `DWAYNE`.
- **`collapse_max` above 1 returns the best documents of each group**, where Xapian returned lower-ranked ones.
- **A search over several indexes weighs each index on its own statistics.** Scores and the order of hits
  change against alpha.10; adding an index to a search no longer reorders the others' hits.
- **Index patterns are globs over path segments**: `*`, `?` and `[...]` never match a `/`, so write
  `test*/*` or `test*/**` for what `test*` used to find below `test1/`. `_all`, wildcards and exclusions work in
  the index list of a search.

### Limits

A request over a limit answers `400` (or the status named) with the limit in the message.

- **Queries**: 100 levels of operators, 4,096 terms and operators (an `_in` or OR over more ids is refused), 256
  phrases per AND, 45 levels of query DSL, 200 levels of body nesting. YAML anchors and aliases (`&name`, `*name`) answer `400`.
- **Aggregations**: 65,535 live buckets per search and 16 levels of nesting.
- **Bodies**: 16 MiB buffered; a larger non-`stored` body, or a chunked `RESTORE`, answers `413`. A `stored`
  blob over 16 MiB streams and needs a `Content-Length`; a `POST` of one without an id answers `400`, and past 4 GiB `413`.
- **Lua scripts**: per call 10^8 instructions, 5x10^8 units of work, 64 MiB of live data and 16x10^6 units in
  one library call (`400`); results over 16 MiB or 262,144 value units answer `413`. Calls of a script no longer share
  globals, an error a script raises answers `400` where it used to store the document untransformed, and `load`,
  `dofile`, `loadfile`, `collectgarbage`, `string.pack`, `string.unpack`, `string.packsize` and `string.dump` are gone.
- **Concurrency**: a search of an index already read by its cap of concurrent searches answers `503`; a loop with 256
  requests waiting answers `503` with `Retry-After: 1`. `--max-search-indexes` (100,000) bounds a search.
- **Paths**: `-D ""`, `PUT /a/../doc`, `/./doc`, a path outside the data directory, and index names with NUL, control
  bytes, a backslash or malformed UTF-8 answer `400` or a usage error. `a/b/..` is the path `a`.

### Running the node

- **One node.** `--solo`, `--cluster`, `--discovery-*`, `--xapian-port`, `--replica-port`, `--primary-node`,
  `--matchers`, `--flush-threshold`, `--replicas`, `--replicators`, `--remote-clients`, `--remote-servers`,
  `--replication-clients`, `--replication-servers`, `--discoverers` and `--database-stall-time` are removed, and so
  are `--committers`, `--fsynchers`, `--http-servers`, `--http-clients`, `--bulk-preparers`, `--bulk-indexers` and
  `--max-database-readers`. They are unknown arguments, so **an old systemd unit or Docker command line exits with a
  usage error** until you drop them. `GET /` has no `cluster_name`, `nodes` or `num_committers`, and `versions` has no
  `Xapian` or `ICU`.
- **`:metrics`** drops the `xapiand_http_clients_*` series and `xapiand_committers_capacity`/`_pool_size`; the
  `#xapian_query` debug field is `#engine_query`. Update dashboards.
- **Shutdown is bounded**: the node exits at most 35 s after the clients are done, with status 75 if it had to abandon
  work (see [Stopping the node](/Xapiand/options#stopping-the-node)). A taken port ends the process with status 70.
- **HTTP is stricter**: a bare LF, obs-fold, a `Transfer-Encoding` other than exactly `chunked` and a bad `Content-Length`
  close the connection without a reply.
- **A write whose commit wait times out succeeds**; the commit is left to the timer.

## Red flags

**Downgrading needs the old data directory.** 0.40.0 cannot read an index that
1.0.0 wrote. Keep the 0.40.0 data directory, or a backup of it, until you no
longer need to step back (see [Rolling back](#rolling-back)).

**Identifiers move to the v6 format.** 1.0.0 mints compact UUID identifiers in a new
format, **cuuid v6**: a UUIDv6-based, splitmix64-reconstructed id that decodes about
120x faster than the old v1 (no per-read Mersenne Twister) at the same wire size. Two
consequences:

- **New ids differ.** Ids the server generates for you in 1.0.0 are v6, so both their
  encoded (`~…`) and canonical (`xxxxxxxx-…`) forms differ from what 0.40.0's v1
  generator would have produced. Any id you supply yourself is stored as given.
- **Old ids still resolve, but their canonical form may render differently.** Ids
  already in your data are matched by their stored bytes, so lookups, links, and the
  `~…` encoded form are unchanged. However, if you read an *old* id back through a
  field-decoding representation (`vanilla`, `urn`, `guid`), 1.0.0 decodes it as v6 and
  produces a different UUID string than 0.40.0 did. Deployments using the default
  encoded (`~…`) representation see no difference; only the canonical reprs do, and
  only for pre-existing ids.

This is a deliberate breaking change for the major-version bump. If you need the exact
old behavior, start Xapiand with **`--legacy-ids`**: it mints v1 and takes the v6 codec
entirely out of the loop, so identifiers behave byte-for-byte as they did in 0.40.0.
It is also the safe rollback if the new format causes any surprise. To make old ids
render their original canonical form under the default mode, reindex the affected
documents so their ids are re-minted as v6.


## Migrating a single node from 0.40.0

The steps are those of [Upgrading from 1.0.0-alpha.9 and 1.0.0-alpha.10](#upgrading-from-100-alpha9-and-100-alpha10),
with the 0.40.0 data directory as the source. Run the final Xapian-only release (the `xapiand` branch) against it
for the dump.

## Rolling back

Stop 1.0.0 and start the 0.40.0 binary against the old data directory. Writes made
on 1.0.0 since the upgrade live only in the new directory.
