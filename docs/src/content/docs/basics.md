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

A write the node answered with a 2xx is in its shard's journal and survives a
crash (see `docs/mechanisms/journal-durability.md`). The index commits on its
own 1 s after its last write (3 s while writes keep coming, and 8 to 10 s after
the first of them at the latest), and `COMMIT` commits it now; a commit bounds
the journal, which a crash replays the next time the index opens.

Writes and reads take their levels as query parameters:

| Parameter | Values | What it asks |
|---|---|---|
| `ack` | `quorum` (default), `none` | `quorum` answers once the write applied, with its result: a `409` when its version check failed. `none` answers `202 Accepted` once the write is in the journal, before it applies: the answer has no `_version`, and a refusal at apply is not reported. |
| `refresh` | `false` (default), `wait_for`, `true` | `true` refreshes the index before the answer, so no read pays for it. `wait_for` answers once every read sees the write, waiting for an `ack=none` write to apply. `false` leaves the refresh to the next read. |
| `read` | `prefix` (default), `linearizable` | `linearizable` waits until every write answered before the read has applied. `prefix` reads what has applied, which may miss an `ack=none` write still on its way. |

A search or a get refreshes its index first, so either `read` sees every write
answered at `ack=quorum`. `?commit` means `ack=quorum&refresh=true`, and another
level beside it answers `400`; `?volatile` means `read=linearizable`. A `RESTORE`
takes `ack` too: at `none` each object's result has no `_version`, and it
answers `202`.

A bulk loader that does not need each answer to carry its result writes at
`ack=none`:

```rest
PUT /concepts/levels/1?ack=none

{
  "title": "Answered once it is in the journal"
}
```

<!-- e2e:begin
---
description: ack=none answers 202 once the write is in the journal
params: commit=false
status: 202
---
e2e:end -->

A `linearizable` read sees it, applied or not when the read arrived:

```rest
GET /concepts/levels/1?read=linearizable
```

<!-- e2e:begin
---
description: A linearizable get sees the ack=none write
---

```js
pm.test("The ack=none write is read back", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().title).to.equal("Answered once it is in the journal");
});
```
e2e:end -->

`refresh=true` refreshes before the answer, `refresh=wait_for` answers once
reads see the write, and `?commit` is `ack=quorum&refresh=true`:

```rest
PUT /concepts/levels/2?refresh=true

{
  "title": "Refreshed before the answer"
}
```

<!-- e2e:begin
---
description: refresh=true answers once refreshed
---
e2e:end -->

```rest
PUT /concepts/levels/3?ack=none&refresh=wait_for

{
  "title": "Answered once reads see it"
}
```

<!-- e2e:begin
---
description: refresh=wait_for at ack=none answers 202 once applied
params: commit=false
status: 202
---
e2e:end -->

```rest
PUT /concepts/levels/4?commit

{
  "title": "ack=quorum and refresh=true"
}
```

<!-- e2e:begin
---
description: ?commit is ack=quorum&refresh=true
---
e2e:end -->

```rest
SEARCH /concepts/levels/?read=prefix
```

<!-- e2e:begin
---
description: A prefix search sees every write once it applied
---

```js
pm.test("The four writes are found", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().hits.length).to.equal(4);
});
```
e2e:end -->

A level the node does not know, or one beside `?commit` that contradicts it,
answers `400`:

```rest
PUT /concepts/levels/5?commit&ack=none

{
  "title": "Refused"
}
```

<!-- e2e:begin
---
description: ?commit with ack=none is refused
status: 400
---
e2e:end -->
