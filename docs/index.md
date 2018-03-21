# Xapiand

A RESTful Search Engine

---

Xapiand aims to be:

**A Highly Available Distributed RESTful Storage and Search
Engine built for the Cloud and with Data Locality in mind.**

* HTTP RESTful API:
	* Document oriented.
	* No need for upfront schema definition.

* Search Engine:
	* Built on top of [Xapian](http://xapian.org/) indexes.

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
	* Written in C++14.

* Geospatial support:
    * Uses The Hierarchical Triangular Mesh for indexing.
    * Accepts multiple Coordinate Reference Systems, including WGS84.
    * Implements EWKT.

* Support for query Aggregations.

* Liberal Open Source license: MIT license (Note Xapian engine itself is GPL).


# Index

[Getting Started](/getting-started)


---

<a id="footnote-1">1</a>: [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)

<a id="footnote-2">2</a>: [The Little Engine(s) That Could: Scaling Online Social Networks.](http://ccr.sigcomm.org/online/files/p375.pdf)
