---
title: Backups
---

Here we'll learn about how to backup your data by making full dumps and restores
using specific commands from the command line, how to dump and restore documents
(in bulk) using the _dump_ and _restore_ endpoints, and we'll see an example
of updating and reindexing documents using different a schema.

---

## Full Dump and Restore

### Dump

A dump of the "twitter" index can be done in three steps:

```sh
# 1. Dump index metadata.
~ $ xapiand --dump-metadata="twitter" --out="twitter.meta"
# 2. Dump index schema.
~ $ xapiand --dump-schema="twitter" --out="twitter.schm"
# 3. Dump index documents.
~ $ xapiand --dump="twitter" --out="twitter.docs"
```

### Restore

The restore for the above dump can also be done in three steps:

```sh
# 1. Restore index metadata.
~ $ xapiand --restore="twitter" --in="twitter.meta"
# 2. Restore index schema.
~ $ xapiand --restore="twitter" --in="twitter.schm"
# 3. Restore index documents.
~ $ xapiand --restore="twitter" --in="twitter.docs"
```

{: .note .warning}
**_Use the same parameters used when running the server_**<br>
For all the above commands you _should_ use the same parameters you use while
running the server. For example, if the server runs in "optimal" mode by using
the `--optimal` flag, also add the same flag to the dump/restore command above.

---

## Online Dump and Restore

It's also possible (for rather small databases) to dump and restore all
documents to and from JSON (or MessagePack) over HTTP.

### Dump

{% capture req %}
```json
POST /twitter/:dump?pretty
```
{% endcapture %}
{% include curl.html req=req %}

### Restore

{% capture req %}
```json
POST /twitter/:restore?pretty

[
  {
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!",
    "_id": 1
  },
  {
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?",
    "_id": 2
  }
]
```
{% endcapture %}
{% include curl.html req=req %}

---

## Restore using different schema

If you need a different or definitive schema for the dumped documents, instead
of restoring the metadata and the schema you may want to put a different schema
for the index to be restored; and then restore the documents to that index:

#### Create a new schema ([foreign]({{ '/docs/reference-guide/schema#foreign' | relative_url }}) in this example) for a new index

{% capture req %}
```json
PUT /new_twitter/:schema

{
  "_type": "foreign/object",
  "_endpoint": ".schemas/00000000-0000-1000-8000-010000000000",
  "_id": {
    "_type": "uuid",
  },
  "description": "Twitter Schema",
  "schema": {
    "_type": "object",
    "_id": {
      "_type": "integer",
    },
    "user": {
      "_type": "keyword"
    },
    "postDate": {
      "_type": "datetime"
    },
    "message": {
      "_type": "text"
    }
  },
}
```
{% endcapture %}
{% include curl.html req=req %}

#### Restore the index documents to the new index

{% capture req %}
```json
POST /twitter/:restore?pretty

[
  {
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!",
    "_id": 1
  },
  {
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?",
    "_id": 2
  }
]
```
{% endcapture %}
{% include curl.html req=req %}
