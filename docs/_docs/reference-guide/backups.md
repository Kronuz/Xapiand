---
title: Backups
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


## Restore using different schema

If you need a different or definitive schema for the dumped documents, instead of
restoring the metadata and the schema (steps *1* and *2*, above) you may want to
put a different schema for the index to be restored; and then restore the
documents to that index:

#### Create a new schema ([foreign]({{ '/docs/reference-guide/schema#foreign' | relative_url }}) in this example) for a new index

{% capture json %}

```json
PUT new_twitter/:schema
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
      "_type": "term"
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
{% include curl.html json=json %}

#### Restore the index documents to the new index

```sh
~ $ xapiand --restore="new_twitter" --in="twitter.docs"
```
