---
title: Welcome
permalink: /docs/home/
redirect_from: /docs/index.html
---

This documentation aims to be a comprehensive guide to Xapiand. We'll cover
topics such as getting your site up and running, indexing and searching
documents, customizing data schemas, deploying to various environments, and
give you some advice on participating in the future development of Xapiand
itself.

## So what is Xapiand, exactly?

Xapiand is *A Highly Available Distributed RESTful Search and Storage
Engine built for the Cloud and with Data Locality in mind*. It takes JSON
documents and indexes them efficiently for later retrieval.

### Xapiand aims to feature:

* Liberal Open Source license:
  * MIT license *(Note Xapian engine itself is GNU GPL)*.

* HTTP RESTful API:
  * Document oriented.
  * No need for upfront schema definition.

* Search Engine:
  * Built on top of [Xapian](http://xapian.org/) indexes.

* Support for query Aggregations.

* Storage Engine:
  * Each index also offers storage of files: à la Facebook's Haystack <sup>[1](#footnote-1)</sup>.

* Multi Tenant with Multi Types:
  * Support for more than one index.
  * Support for different types, one per index.
  * Index level configuration:
    * Schema.

* Efficient and Scalable architecture:
  * (Near) Real Time Search.
  * Event driven asynchronous architecture using [libev](http://software.schmorp.de/pkg/libev.html).
  * Written in modern C++.

* Geo-spatial support:
    * Uses The Hierarchical Triangular Mesh for indexing.
    * Accepts multiple Coordinate Reference Systems, including WGS84.
    * Implements EWKT.

* Multi-Partitioning and Distribution Strategies:
  * **NOT YET FULLY IMPLEMENTED**
  * Social-Based Partitioning and Replication (SPAR <sup>[2](#footnote-2)</sup>).
  * Random Consistent Partitioning and Replication.

* Highly Available:
  * **NOT YET FULLY IMPLEMENTED**
  * Automatic node operation rerouting.
  * Replicas exists to maximize high availability *and* data locality.
  * Read and Search operations performed on any of the replicas.
  * Reliable, asynchronous replication for long term persistence.


---

If you come across anything along the way that we haven't covered, or if you
know of a tip you think others would find handy, please [file an
issue]({{ site.repository }}/issues/new) and we'll see about
including it in this guide.


---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)

<sup><a id="footnote-2">2</a></sup> [The Little Engine(s) That Could: Scaling Online Social Networks.](http://ccr.sigcomm.org/online/files/p375.pdf)
