---
title: About
read_only: true
permalink: /about/
---

[Xapian]: https://xapian.org
[GitHub]: https://github.com/Kronuz/Xapiand

Xapiand is a _fast_, _simple_ and _modern_ search and storage server built for the cloud.
It features:

##### Liberal Open Source license:
  * MIT license *(Note Xapian engine itself is GNU GPL)*.
  * You can find the source code for Xapiand at [GitHub]

##### HTTP RESTful API:
  * Document oriented.
  * No need for upfront schema definition.

##### Lightweight Engine:
  * Small footprint with very low memory usage.

##### Search Server:
  * Built on top of [Xapian]{:target="_blank"} indexes.

##### Storage Server:
  * Each index also offers storage of files: à la Facebook's Haystack <sup>[1](#footnote-1)</sup>.

##### Support for query Aggregations:
  * Metrics aggregations.
  * Bucket aggregations.

##### Multi Tenant with Multi Types:
  * Support for more than one index.
  * Support for different types, one per index.
  * Index level configuration:
    * Schema.

##### Efficient and Scalable architecture:
  * (Near) Real Time Search.
  * Event driven asynchronous architecture using [libev](http://software.schmorp.de/pkg/libev.html){:target="_blank"}.
  * Written in modern C++.

##### Geo-spatial support:
  * Uses the Hierarchical Triangular Mesh <sup>[2](#footnote-2)</sup> for indexing.
  * Accepts multiple Coordinate Reference Systems, including WGS84.
  * Implements EWKT.

##### High Availability:
  * Automatic node operation rerouting.
  * Replicas exists to maximize high availability *and* data locality.
  * Read and Search operations performed on any of the replicas.
  * Reliable, asynchronous replication for long term persistence.

##### Multi-Partitioning and Distribution Strategies:
  * Random Consistent Partitioning and Replication.

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}

<sup><a id="footnote-2">2</a></sup> [The Hierarchical Triangular Mesh.](http://www.noao.edu/noao/staff/yao/sdss_papers/kunszt.pdf){:target="_blank"}


---

## Core Team

*The {{ site.name }} Core Team's responsibility is to ensure the development and
community around the {{ site.name }} ecosystem thrive.*

* [Germán Méndez Bravo (Kronuz)](https://kronuz.io)
* [José Madrigal Cárdenas (YosefMac)](https://github.com/YosefMac){:target="_blank"}
* [José María Valencia Ramírez (JoseMariaVR)](https://github.com/JoseMariaVR){:target="_blank"}
