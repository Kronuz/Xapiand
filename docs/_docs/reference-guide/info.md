---
title: Information API
short_title: Info API
---

You can retrieve information about the server, about a database, about a
particular document, or about the state of the cluster and the server
using the _Information API_.


### Server Information

The most basic kind of information you can get is the server information:

{% capture req %}

```json
GET /?pretty
```
{% endcapture %}
{% include curl.html req=req %}

The response contains:

* `name`           - Name of the node.
* `cluster_name`   - Name of the cluster.
* `server`         - Server version string.
* `versions`       - Versions of the internal libraries.


### Database Information

The simplest form gets information about a database:

{% capture req %}

```json
GET /bank/:info?pretty
```
{% endcapture %}
{% include curl.html req=req %}

Response will include the following information about the database:

* `uuid`          - UUID of the xapian database.
* `revision`      - Internal document ID.
* `doc_count`     - Documents count.
* `last_id`       - Last used ID.
* `doc_del`       - Documents deleted.
* `av_length`     - Average length of the documents.
* `doc_len_lower` - Statistics about documents length.
* `doc_len_upper` - Statistics about documents length.
* `has_positions` - Boolean indicating if the database has positions stored.


### Document Information

To also retrive information about a given document, pass the document ID as
part of the GET query:

{% capture req %}

```json
GET /bank/:info/1?pretty
```
{% endcapture %}
{% include curl.html req=req %}

In addition to the database information, the response will also include a set
of valuable information about the required document:

* `docid`      - Internal document ID.
* `data`       - Content types stored in the document data.
* `terms`      - Object containing all the terms indexed by the document.
* `values`     - Object containing all values stored by the document.


### Monitoring

Other type of information you can retrieve is information aboout the current
state of the server. This can be retrived using the `:metrics` endpoint,
explained in its own section: [Monitoring Xapiand](../monitoring).
