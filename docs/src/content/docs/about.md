---
title: "About"
---

[strata]: https://github.com/Kronuz/strata
[GitHub]: https://github.com/Kronuz/Xapiand

Xapiand is a _fast_, _simple_ and _modern_ search and storage server built for the cloud.
It features:

##### Liberal Open Source license:
  * MIT license (a few files carry their own; see the [Copyright Notice](/Xapiand/copyright)).
  * You can find the source code for Xapiand at [GitHub]

##### HTTP RESTful API:
  * Document oriented.
  * No need for upfront schema definition.

##### Lightweight Engine:
  * Small footprint with very low memory usage.

##### Search Server:
  * Built on top of [strata] indexes.

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
  * Event driven asynchronous architecture using the Asio-based [reactor](https://github.com/Kronuz/reactor) library.
  * Written in modern C++.

##### Geo-spatial support:
  * Uses the Hierarchical Triangular Mesh <sup>[2](#footnote-2)</sup> for indexing.
  * Accepts multiple Coordinate Reference Systems, including WGS84.
  * Implements EWKT.

##### Sharding:
  * Each index splits into shards on a single node (`number_of_shards`).

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)

<sup><a id="footnote-2">2</a></sup> [The Hierarchical Triangular Mesh.](http://www.noao.edu/noao/staff/yao/sdss_papers/kunszt.pdf)

---

## Core Team

*The Xapiand Core Team's responsibility is to ensure the development and
community around the Xapiand ecosystem thrive.*

* [Germán Méndez Bravo (Kronuz)](https://Kronuz.github.io)
* [José Madrigal Cárdenas (YosefMac)](https://github.com/YosefMac)
* [José María Valencia Ramírez (JoseMariaVR)](https://github.com/JoseMariaVR)
