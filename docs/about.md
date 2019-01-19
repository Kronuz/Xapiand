---
layout: page
title: About
permalink: /about/
---

[Xapian]: https://xapian.org
[GitHub]: https://github.com/Kronuz/Xapiand

Xapiand is a _fast_, _simple_ and _modern_ search and storage server built for the cloud.
It features:

##### Lightweight engine
  * Small footprint with very low memory usage.

##### Liberal Open Source license:
  * MIT license *(Note Xapian engine itself is GNU GPL)*.
  * You can find the source code for Xapiand at [GitHub]

##### HTTP RESTful API:
  * Document oriented.
  * No need for upfront schema definition.

##### Search Engine:
  * Built on top of [Xapian]{:target="_blank"} indexes.

##### Storage Engine:
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


---

## {{ site.name }} License

Copyright &copy; 2015-2019 Dubalu LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.


---

## {{ site.name }} Documentation License

Copyright &copy; 2015-2019 Dubalu LLC<br>
Copyright &copy; 2009-2018 Elasticsearch

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0){:target="_blank"}

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
