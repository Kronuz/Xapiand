---
title: Info API
---

You can retrieve information about a database or a particular document using the
`:info` API.


### Database Info Example

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


### Document Info Example

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
