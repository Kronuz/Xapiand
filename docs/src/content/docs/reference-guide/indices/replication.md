---
title: "Data Replication"
---

Each index in Xapiand is divided into [shards](/Xapiand/basics#shard)
and each shard can have multiple copies. These copies are known as a replicas
and must be kept in sync when documents are added or removed. If we fail to do
so, reading from one copy will result in very different results than reading
from another. The process of keeping the shard copies in sync and serving reads
from them is what we call the data replication.

## Primary and Replica Shards

Every document belongs to a single **primary shard**, chosen from the document's
identifier, so the number of primary shards fixes how the data is partitioned and
cannot change after the index is created. A **replica shard** is a full copy of a
primary shard.

Writes always land on the primary shard first and are then copied to that shard's
replicas, so a document can be read back from its primary or from any of its
replicas. Replicas therefore do two jobs at once: they protect against losing a
node, and they add read capacity, because a search or a document fetch can be
answered by any copy.

The number of replicas is set with `number_of_replicas` and, unlike the shard
count, it can be changed on a live index.

## Configuring an index's shards and replicas

Both counts are given under `_settings` when the index is first written to:

```rest
PUT /test_replication/

{
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 1
  }
}
```

Indexing a document materializes the index across those shards. We pass `commit`
here only so the write is immediately visible to the next request (see
[Commit](/Xapiand/reference-guide/documents/index-api/#commit)
— prefer letting writes auto-commit in production):

```rest
PUT /test_replication/1?commit

{
  "title": "Data replication",
  "body": "keeping shard copies in sync"
}
```

The index's [`:info`](/Xapiand/reference-guide/indices)
reports the shards the data was partitioned into and the document count:

```rest
GET /test_replication/:info
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Index was partitioned into 3 shards", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.shards.length).to.equal(3);
});
```

```js
pm.test("Document was indexed", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.doc_count).to.equal(1);
});
```
e2e:end -->

## How replication works

Replication runs over its own protocol, separate from the REST API and from the
[Remote protocol](/Xapiand/reference-guide/indices) that
serves distributed searches. It listens on its own TCP port (`--replica-port`) and
is driven by a small pool of workers: `--replicators` trigger replication when a
primary changes, while `--replication-servers` and `--replication-clients` carry
the transfer.

When a replica needs to catch up, it brings its copy of the shard's database in
line with the primary, and replays the shard's
[write-ahead log](/Xapiand/reference-guide/wal) to apply the
changes committed since the copy was taken. A brand-new or badly out-of-date
replica is seeded from a full copy of the primary's database files; from then on
it only needs the incremental changes.

## Where replicas live, and why it matters

A node that holds a copy of a shard reads it **locally**; a node that does not
must read it from a holder over the Remote protocol. So the more replicas a shard
has, the more nodes can answer reads for it from local storage, and the more read
throughput the index can sustain. Adding replicas beyond the number of nodes does
not help, since a node gains nothing from holding two copies of the same shard.

Because a `--solo` node has no peers to copy to, replication only takes effect in
a cluster. See [Clustering](/Xapiand/clustering) for how
shards and replicas are allocated across nodes, how a new master is elected, and
how the cluster copes with a node failing.
