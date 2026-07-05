---
title: Upgrading
short_title: Upgrading
---

This guide covers upgrading an existing deployment from **0.40.0** to **1.0.0**.
The short version: your data carries over untouched, a single node upgrades in
place with no reindex, and the only thing you cannot do is mix the two versions in
one running cluster.


## Data compatibility

Upgrading is seamless. The Xapian glass backend carries the same on-disk format
version, index schemas and metadata are read back as written, and 1.0.0 still reads
the LZ4-compressed document data that 0.40.0 wrote. A 1.0.0 node opens a database
that 0.40.0 wrote, serves searches against it, and indexes new documents into it,
all without a reindex or a conversion step.

Downgrading is a different story. 1.0.0 compresses new document data with
Zstandard, tagged so the reader knows which codec to use; 0.40.0 predates that and
only understands LZ4, so it cannot read any document that 1.0.0 has written or
updated. Existing documents that 1.0.0 has only read are untouched and remain
readable by 0.40.0, but as soon as 1.0.0 writes, that data is one-way. Treat the
upgrade as forward-only and rely on backups, not on a binary swap, to step back
(see [Rolling back](#rolling-back)).

**What carries over on upgrade without any action:**

- The index itself: postlists, term lists, positions, values, spelling, synonyms.
- Document data, including the LZ4-compressed large-value form 0.40.0 wrote.
- Index schemas, including dynamic types and namespaces.
- Query behaviour: the same query returns the same documents.


## Red flags

**A cluster must be upgraded as a whole.** The internal node-to-node protocol moved
from version 44 to version 47. When two nodes disagree on the protocol version the
handshake refuses the connection with a clear error rather than guessing at the
wire format:

```
Server supports protocol version 44.0 - client is using 47.0
```

This is deliberate. A silent version mismatch would misread message bodies and
corrupt replication; a loud refusal keeps the cluster honest. The practical
consequence is that **you cannot do a rolling, node-by-node upgrade** of a live
cluster: a 0.40.0 node and a 1.0.0 node will not form a cluster together. Stop the
whole cluster, upgrade every node, then bring it back up.

**Downgrading is one-way after the first write.** 1.0.0 writes document data with
Zstandard compression, which 0.40.0 cannot decode. Reading with 1.0.0 changes
nothing, but once 1.0.0 indexes or updates a document, that document is no longer
readable by 0.40.0. A binary rollback is therefore only safe if 1.0.0 never wrote;
otherwise, restore from a backup taken before the upgrade.

**Auto-generated compact ids look different (cosmetic).** The server-side generator
for compact UUID identifiers had a salt bug fixed in 1.0.0, so the *encoded form* of
ids that 1.0.0 generates for you differs from what 0.40.0 would have produced. This
only affects new ids the server mints on your behalf; ids already stored in your
data are read back exactly as written, and any id you supply yourself is unaffected.


## Migrating a single node

A single node upgrades in place:

1. Stop the running node and let it shut down cleanly (a clean stop checkpoints the
   write-ahead log, so there is nothing version-specific left to replay).
2. Replace the `xapiand` binary with the 1.0.0 build.
3. Start the node against the **same** data directory.

There is no reindex and no migration command. The first start reads the existing
database as-is.


## Migrating a cluster

Because the two versions will not interoperate on the wire, upgrade the cluster in
one window rather than one node at a time:

1. Stop **all** nodes and let each drain and shut down cleanly.
2. Replace the binary on every node.
3. Start the nodes back up against their existing data directories.

Each node reads its local shards unchanged, rejoins discovery, and replication
resumes. If your deployment cannot tolerate a full-cluster stop, stand up a second
cluster on 1.0.0 and re-index into it from the source of truth, then cut traffic
over.


## Rolling back

Plan the upgrade as forward-only. A 0.40.0 binary can reopen a data directory that
1.0.0 only *read*, but any document 1.0.0 has written or updated uses Zstandard
compression that 0.40.0 cannot decode, so a straight binary rollback strands that
data. If you need the option to return to 0.40.0, take a backup before upgrading and
restore from it; do not rely on swapping the binary back once 1.0.0 has served
writes. As with the upgrade, roll a cluster back as a whole, never one node at a
time.
