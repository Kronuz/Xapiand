---
title: Basic Concepts
---

There are a few concepts that are core to Xapiand. Understanding these
concepts from the outset will tremendously help ease the learning process.

---

## Document

A document is a basic unit of information that can be indexed. For example, you
can have a document for a _single customer_, another document for a
_single product_, and yet another for a _single order_. This document is
expressed in [JSON](https://www.json.org){:target="_blank"} (JavaScript Object
Notation) which is a ubiquitous internet data interchange format, but internally
stored as [MessagePack](https://msgpack.org){:target="_blank"}.

A document physically resides in an _index_ and within an index, you can store
as many documents as you want.


## Index

An index (or _database_) is a collection of documents that have somewhat similar
characteristics. For example, you can have an index for _customer data_, another
index for a _product catalog_, and yet another index for order data. An index is
identified by a name and this name is used to refer to the index when performing
_indexing_, _search_, _update_, and _delete_ (CRUD) operations against the
documents in it.

In a single cluster, you can define as many indexes as you want.


## Cluster

A cluster is a collection of one or more _nodes_ (Xapiand servers) that together
holds your entire data and provides federated indexing and search capabilities
across all nodes. A cluster is identified by a unique name which by default is
"_Xapiand_". This name is important because a node can only be part of a cluster
if the node is set up to join the cluster by its name.

Make sure that you don't reuse the same cluster names in different environments,
otherwise you might end up with nodes joining the wrong cluster. For instance
you could use _logging-dev_, _logging-stage_, and _logging-prod_ for the
development, staging, and production clusters, respectively.

Note that it is valid and perfectly fine to have a cluster with only a single
node in it. Furthermore, you may also have multiple independent clusters each
with its own unique cluster name.


## Node

A node is a single Xapiand server that is part of your cluster, stores your
data, and participates in the cluster's indexing and search capabilities. Just
like a cluster, a node is identified by a name which by default is a random
generated _friendly_ name that is assigned to the node at startup. You can
define any node name you want if you do not want the default. This name is
important for administration purposes where you want to identify which servers
in your network correspond to which nodes in your Xapiand cluster.

A node can be configured to join a specific cluster by the cluster name. By
default, each node is set up to join a cluster named _Xapiand_ which means that
if you start up a number of nodes on your network and—assuming they can discover
each other—they will all automatically form and join a single cluster named
_Xapiand_.

In a single cluster, you can have as many nodes as you want. Furthermore, if
there are no other Xapiand nodes currently running on your network, starting a
single node will by default form a new single-node cluster named _Xapiand_.


## Shard

An index can potentially store a _large amount of data_ that can exceed the
hardware limits of a single node. For example, a single index of a billion
documents taking up 1TB of disk space may not fit on the disk of a single node
or may be too slow to serve search requests from a single node alone. To solve
this problem, Xapiand provides the ability to subdivide your index into multiple
pieces called _shards_. Each shard is in itself a fully-functional and
independent "index" that can be hosted on any node in the cluster.

Sharding is important for two primary reasons:

- It allows you to _horizontally split/scale_ your content volume.
- It allows you to _distribute_ and _parallelize_ operations across
  shards (potentially on multiple nodes) thus increasing performance/throughput.

The mechanics of how a shard is distributed and also how its documents are
aggregated back into search requests are completely managed by Xapiand and is
transparent to you as the user.

When you create an index, you can simply define the number of shards that you
want. After the index is created, you can change the number of shards for an
existing index, however this is not a trivial task and pre-planning for the
correct number of shards is the optimal approach.

By default, each index in Xapiand is allocated 5 primary shards.


## Replica

In a network/cloud environment where failures can be expected anytime, it is
very useful and highly recommended to have a failover mechanism in case a
shard/node somehow goes offline or disappears for whatever reason. To this end,
Xapiand allows you to make one or more copies of your index's shards into what
are called _replica shards_, or _replicas_ for short.

An index can also be replicated zero (meaning no replicas) or more times. Once
replicated, each index will have primary shards (the original shards that were
replicated from) and replica shards (the copies of the primary shards).

Replication is important for two primary reasons:

- It provides _high availability_ in case a shard/node fails. For this reason,
  it is important to note that a replica shard is never allocated on the same
  node as the original/primary shard that it was copied from.
- It allows you to scale out your search volume/throughput since searches can
  be executed on all replicas in parallel.

The number of replicas per shard can be defined per index at the time the index
is created. After the index is created, you may also change the number of
replicas dynamically anytime.

By default, each index in Xapiand is allocated 1 replica which means that, if
you don't modify the defaults and you have at least two nodes in your cluster,
your index will have 5 primary shards and another 5 replica shards (1 complete
replica) for a total of 10 shards per index.


## Near Realtime (NRT)

An important thing to notice with Xapiand is that it is a _near-realtime_ search
platform. What this means is there is a slight latency (normally a couple seconds)
from the time you index a document until the time it becomes searchable.
