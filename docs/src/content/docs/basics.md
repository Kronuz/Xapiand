---
title: "Basic Concepts"
---

There are a few concepts that are core to Xapiand. Understanding these
concepts from the outset will tremendously help ease the learning process.

---

## Document

A document is a basic unit of information that can be indexed. For example, you
can have a document for a _single customer_, another document for a
_single product_, and yet another for a _single order_. This document is
expressed in [JSON](https://www.json.org) (JavaScript Object
Notation) which is a ubiquitous internet data interchange format, but internally
stored as [MessagePack](https://msgpack.org).

A document physically resides in an _index_ and within an index, you can store
as many documents as you want.

## Index

An index (or _database_) is a collection of documents that have somewhat similar
characteristics. For example, you can have an index for _customer data_, another
index for a _product catalog_, and yet another index for order data. An index is
identified by its path and this path is used to refer to the index when
performing _indexing_, _search_, _update_, and _delete_ (CRUD) operations
against the documents in it.

In `/twitter/user/Kronuz`, the index is `twitter/user` and the document ID is
`Kronuz`: the last part of the path is the ID, and everything before it names
the index. Indexes nest, so `twitter` and `twitter/user` can both exist as
indexes, and one search can name many of them by list, wildcard or `_all` (see
[Multiple Indexes](/Xapiand/reference-guide/api#multiple-indexes)).

On a single node, you can define as many indexes as you want. Writing to an
index that does not exist creates it.

## Node

A node is a single Xapiand server that stores your data and serves indexing
and search requests. A node is identified by a name which by default is a
randomly generated _friendly_ name assigned at startup. You can set any name
you want with `--name`; it helps identify which server is which in your logs.

Xapiand runs as a single node: every index lives in one node's data directory,
and the cluster that shared indexes and replicated them across nodes is not part
of this release. Options and settings that belonged to the cluster no longer
exist (see [Removed options](/Xapiand/options#removed-options)).

## Shard

An index can potentially store a _large amount of data_. To spread that work,
Xapiand can subdivide your index into multiple pieces called _shards_. Each
shard is in itself a fully-functional and independent "index" on the node.

Sharding splits an index's content into smaller pieces. How documents are
routed to a shard, and how results are aggregated back into search requests, is
completely managed by Xapiand and is transparent to you as the user.

When you create an index, you can define the number of shards that you want
with `number_of_shards`. The number of shards is fixed once the index is
created, so pre-planning the correct number of shards ahead of time is the
optimal approach.

By default, each index in Xapiand is allocated 5 shards (`--shards` sets another
default for the node). A shard is stored as the index path followed by
`.__<number>`, which is how `INFO` lists them:

```rest
PUT /concepts/sharded/

{
  "_settings": {
    "number_of_shards": 3
  }
}
```

<!-- e2e:begin
---
description: Create an index with three shards
---

```js
pm.test("The index is created", function () {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
INFO /concepts/sharded/
```

<!-- e2e:begin
---
description: The index has three shards
---

```js
pm.test("The index has the three shards that were asked for", function () {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.shards).to.eql([
    "concepts/sharded/.__1",
    "concepts/sharded/.__2",
    "concepts/sharded/.__3"
  ]);
});
```
e2e:end -->

An index created without `number_of_shards` gets the node's default:

```rest
PUT /concepts/default/1

{
  "title": "An index with the default number of shards"
}
```

```rest
INFO /concepts/default/
```

<!-- e2e:begin
---
description: The index has the default five shards
---

```js
pm.test("The index has five shards", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().shards.length).to.equal(5);
});
```
e2e:end -->

## Visibility and Durability

A write is visible to the next read as soon as the node has answered it: a
search or a get refreshes its index first, so a request reads its own writes.
Durability comes in two steps. The node appends every write to the index's
write-ahead log before it answers (see [Write-Ahead Log](/Xapiand/reference-guide/wal)),
and the index commits 1 s after its last write (3 s while writes keep coming, and
8 to 10 s after the first of them at the latest). A `?commit` on the request
commits before the answer. A crash replays the log the next time the index
opens.
