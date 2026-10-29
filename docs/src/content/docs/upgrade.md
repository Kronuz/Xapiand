---
title: "Upgrading"
---

This guide covers upgrading an existing node from **0.40.0** to **1.0.0**.
The short version: 1.0.0 searches with the [strata](https://github.com/Kronuz/strata)
engine, which does not read indexes written by the Xapian engine, so your documents
move over with a [dump and a restore](/Xapiand/reference-guide/indexes/backups).
Xapiand runs as a single node; the cluster returns in a later release.

## Data compatibility

1.0.0 reads only indexes that the strata engine wrote. An index written by 0.40.0
(or by an earlier 1.0.0 alpha on its default Xapian engine) holds Xapian's on-disk
format, which 1.0.0 cannot open. Carry the documents over instead: dump each index
with the final Xapian-only release (the `xapiand` branch) and restore it into 1.0.0.

A dump holds every document's data with its `_id` and `_version`. Index settings
and explicit schemas are set again on the new node, before the restore (see [Restore using different schema](/Xapiand/reference-guide/indexes/backups#restore-using-different-schema)).

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

## Migrating a single node

1. Run the final Xapian-only release (the `xapiand` branch) against the 0.40.0 data
   directory and dump every index (`DUMP`, or `xapiand --dump` with the server's
   parameters; see [Backups](/Xapiand/reference-guide/indexes/backups)).
2. Stop the node and let it shut down cleanly.
3. Start the 1.0.0 binary against a **new, empty** data directory (`-D`), and keep
   the old one untouched.
4. Create each index with its settings and schema, if it had explicit ones.
5. Restore every dump (`RESTORE`, or `xapiand --restore`).

## Rolling back

Stop 1.0.0 and start the 0.40.0 binary against the old data directory. Writes made
on 1.0.0 since the upgrade live only in the new directory.
