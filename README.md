# Xapiand


## A RESTful Search Engine

Xapiand aims to be: A Highly Available Distributed RESTful Storage and Search Engine built for the Cloud and with Data Locality in mind.

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

* Liberal Open Source license: MIT license (Xapian itself is GPL).


## Getting Started

This guide will take you through the process of installing Xapiand and familiarize you with the concepts that will allow you to use the storage and search indexes. **DON'T PANIC**, it will take just a few minutes.


### Installation

#### Requirements

Xapiand is written in C++14, it makes use of libev (which is included in the codebase). The only external dependencies for building it are:

* Clang or GCC
* pkg-config
* CMake
* libpthread (internally used by the Standard C++ thread library)
* xapian-core v1.4+ (With patches by Kronuz applied, see https://github.com/Kronuz/xapian)
* Optionally, Google's V8 Javascript engine library (tested with v5.1)


#### Building from Source (GitHub)

1. Download and untar the Xapiand official distribution or clone repository from GitHub.

2. Build and install using:

	```
	mkdir build
	cd build
	cmake -GNinja ..
	ninja
	ninja install
	```

3. Run `xapiand` inside a new directory to be assigned to the node.

4. Run `curl 'http://localhost:8880/'`.


### Indexing

Let's try and index some twitter like information. First, let's create a twitter user, and add some tweets (the twitter index will be created automatically):

```
curl -XPUT 'http://localhost:8880/twitter/user/Kronuz?commit' -d '{ "name" : "German M. Bravo" }'

curl -XPUT 'http://localhost:8880/twitter/tweet/1?commit' -d '
{
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!"
}'

curl -XPUT 'http://localhost:8880/twitter/tweet/2?commit' -d '
{
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?"
}'
```

Now, let’s see if the information was added by GETting it:

```
curl 'http://localhost:8880/twitter/user/Kronuz?pretty'
curl 'http://localhost:8880/twitter/tweet/1?pretty'
curl 'http://localhost:8880/twitter/tweet/2?pretty'
```

*TODO: Work in progress...*


### Searching

Let’s find all the tweets that Kronuz posted:

```
curl 'http://localhost:8880/twitter/tweet/.search?q=user:Kronuz&pretty'
```

*TODO: Work in progress...*


### Storage Engine

The storage is designed to put files in volumes much in the way Facebook's
Haystack <sup>[1](#footnote-1)</sup> works; once there a file enters the
storage it can't really get deleted/modified from the volume, but instead, if a
change is needed, a new file blob will be written to the volume. Storage is
envisioned to be used when there are files you need to store which you know
won't be changing often.

Lets put something in the storage using PUT:

```
curl -XPUT -H "Content-Type: image/png" 'http://localhost:8880/twitter/images/Kronuz.png?commit' --data-binary @'Kronuz.png'
```

And getting it is just a matter of retreiving it using GET:

```
curl -H "Accept: image/png" 'http://localhost:8880/twitter/images/Kronuz.png'
```

*TODO: Work in progress...*


### Where to go from here?

*TODO: Work in progress...*


## The road ahead

This is a list of features that are only partially implemented; but that are planned to be supported
by Xapiand some time soonish in order to get closer to the final product:

* Multi-Partitioning and Distribution Strategies:
	* Social-Based Partitioning and Replication (SPAR <sup>[2](#footnote-2)</sup>).
	* Random Consistent Partitioning and Replication.

* Highly Available:
	* Automatic node operation rerouting.
	* Replicas exists to maximize high availability *and* data locality.
	* Read and Search operations performed on any of the replicas.
	* Reliable, asynchronous replication for long term persistency.


### Multi-Partitioning and Distribution Strategies

To achieve high availability, distribution of data and data locality, Xapiand can partition, replicate and distribute indexes across several nodes using any of the following partitioning strategies:


#### Social-Based Partitioning and Replication

* Horizontal scaling by distributing indexes among several nodes.
* Maximizes data locality ensuring related indexes are kept (or are directly available) in the same node.
* Minimizes network usage when accessing a set of related indexes.


#### Random Consistent Partitioning

* Horizontal scaling by distributing indexes among several nodes.


## License

```
Copyright (C) 2015-2017 deipi.com LLC and contributors. All rights reserved.

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
```

---

<a id="footnote-1">1</a>: [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)

<a id="footnote-2">2</a>: [The Little Engine(s) That Could: Scaling Online Social Networks.](http://ccr.sigcomm.org/online/files/p375.pdf)
