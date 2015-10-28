# Xapiand


## A Multi-Partitioning RESTful Search Engine

Xapiand: A Highly Available Distributed RESTful Storage and Search Engine built for the cloud and with Data Locality in mind. To explain:

* HTTP RESTful API:
	* Document oriented.
	* Automatic node operation rerouting.
	* No need for upfront schema definition.
* Storage and Search Engine:
	* Built on top of [Xapian](http://xapian.org/) indexes.
	* Each index also offers storage of files: à la Facebook's Haystack <sup>[1](#footnote-1)</sup>.
* Multi Tenant with Multi Types:
	* Support for more than one index.
	* Support for more than one type per index.
	* Index level configuration:
		* Schema.
		* Replicas.
		* Partitioning strategies.
* Multi-Partitioning and Distribution Strategies:
	* Random Consistent Partitioning and Replication.
	* Social-Based Partitioning and Replication (SPAR <sup>[2](#footnote-2)</sup>).
* Highly Available:
	* Replicas exists to maximize high availability *and* data locality.
	* Read and Search operations performed on any of the replicas.
	* Reliable, asynchronous replication for long term persistency.
* Efficient and Scalable architecture:
	* (Near) Real Time Search.
	* Event driven asynchronous architecture using [libev](http://software.schmorp.de/pkg/libev.html).
	* Written in C++14.
* Liberal Open Source license: MIT license (Xapian itself is GPL).


## Getting Started

This guide will take you through the process of installing Xapiand and familiarize you with the concepts that will allow you to use the storage and search indexes. **DON'T PANIC**, it will take just a few minutes.


### Installation

#### Requirements

Xapiand is written in C++14, it makes use of libpcre and libev (both of which are included in the codebase). The only external dependencies for building it are:

* Clang or GCC
* automake
* autoconf
* libtools
* pkg-config
* libpthread (internally used by the Standard C++ thread library)
* xapian-core v1.3+ (With patches by Kronuz applied, see https://github.com/Kronuz/xapian)


#### Building from Source (GitHub)

1. Download and untar the Xapiand official distribution or clone repository from GitHub.

2. Build and install using:

	```
	./autogen.sh
	./configure
	make
	make install
	```

3. Run `xapiand` inside a new directory to be assigned to the node.

4. Run `curl -X GET http://localhost:8880/`.

5. Start more nodes.


### Indexing

*TODO: Work in progress...*

Let's try and index some twitter like information. First, let's create a twitter user, and add some tweets (the twitter index will be created automatically):

```
curl -XPUT 'http://localhost:8880/twitter/user/Kronuz?commit=1' -d '{ "name" : "German M. Bravo" }'

curl -XPUT 'http://localhost:8880/twitter/tweet/1?commit=1' -d '
{
    "user": "Kronuz",
    "postDate": "2015-11-15T13:12:00",
    "message": "Trying out Xapiand, so far so good?"
}'

curl -XPUT 'http://localhost:8880/twitter/tweet/2?commit=1' -d '
{
    "user": "Kronuz",
    "postDate": "2015-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?"
}'
```

Now, let’s see if the information was added by GETting it:

```
curl -XGET 'http://localhost:8880/twitter/user/Kronuz?pretty=true'
curl -XGET 'http://localhost:8880/twitter/tweet/1?pretty=true'
curl -XGET 'http://localhost:8880/twitter/tweet/2?pretty=true'
```

### Searching

*TODO: Work in progress...*

Let’s find all the tweets that Kronuz posted:

```
curl -XGET 'http://localhost:8880/twitter/tweet/_search?q=user:Kronuz&pretty=true'
```


### Storage

*TODO: Work in progress...*

### Highly Available Data Partitioning

To achieve high availability, distribution of data and data locality, Xapiand can partition, replicate and distribute indexes across several nodes using any of the following partitioning strategies:


#### Social-Based Partitioning and Replication

* Horizontal scaling by distributing indexes among several nodes.
* Maximizes data locality ensuring related indexes are kept (or are directly available) in the same node.
* Minimizes network usage when accessing a set of related indexes.


#### Random Consistent Partitioning

* Horizontal scaling by distributing indexes among several nodes.


### Where to go from here?

*TODO: Work in progress...*


## License

```
Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.

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
