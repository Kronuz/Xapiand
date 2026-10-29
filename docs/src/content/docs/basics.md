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
identified by a name and this name is used to refer to the index when performing
_indexing_, _search_, _update_, and _delete_ (CRUD) operations against the
documents in it.

On a single node, you can define as many indexes as you want.

## Node

A node is a single Xapiand server that stores your data and serves indexing
and search requests. A node is identified by a name which by default is a
randomly generated _friendly_ name assigned at startup. You can set any name
you want with `--name`; it helps identify which server is which in your logs.

Xapiand runs as a single node. The cluster, with nodes that share indexes and
replicate them, returns in a later release.

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

By default, each index in Xapiand is allocated 5 shards.

## Near Realtime (NRT)

An important thing to notice with Xapiand is that it is a _near-realtime_ search
platform. What this means is there is a slight latency (normally a couple seconds)
from the time you index a document until the time it becomes searchable.
