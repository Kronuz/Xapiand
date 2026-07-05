---
title: WAL
---

Every shard keeps a **Write-Ahead Log** (WAL): an append-only record of the
operations applied to it (document writes and deletes, spelling and synonym
updates, and commits), written before those changes are folded into the shard's
main database files. Each entry is tagged with the database revision it produced,
so the log is an ordered, replayable history of the shard.

The WAL exists so that a change is never lost between the moment it is accepted
and the moment it is fully written to disk. A full
[commit]({{ '/docs/reference-guide/documents/index-api/#commit' | relative_url }})
is comparatively expensive, so Xapiand does not perform one on every write;
instead the operation is durably appended to the WAL first and the heavier commit
happens in the background (or when you ask for it explicitly). If a node stops
uncleanly before that commit, it recovers on startup by replaying the WAL from the
last committed revision forward, re-applying exactly the operations that had not
yet made it into the database.


## The WAL and replication

The same replayable history is what
[data replication]({{ '/docs/reference-guide/indices/replication' | relative_url }})
is built on. Once a replica holds a copy of a shard at some revision, it stays in
sync by replaying the primary's WAL entries recorded after that revision, rather
than copying the whole database again. Only a brand-new or far-behind replica
needs a full copy of the database files to seed from.


## Writing the log

WAL entries are written by a pool of asynchronous writer threads so that logging a
change stays off the request's critical path. Two options tune this pool:

- `--writers` sets the number of asynchronous WAL writer threads.
- `--wal-writer-cache-size` sets the writer's cache size.

The WAL is enabled by default. Certain internal paths that manage their own
durability write with it disabled, most notably a replica being seeded from a full
database copy, which does not need to log operations it is receiving wholesale.
