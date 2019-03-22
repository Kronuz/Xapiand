---
title: Index Information API
short_title: Index Info API
---

The simplest form of `:info` gets information about an index:

{% capture req %}

```json
GET /bank/:info?pretty
```
{% endcapture %}
{% include curl.html req=req %}

Response will include the following information about the index:

* `uuid`          - UUID of the xapian database.
* `revision`      - Internal document ID (shown only for shard databases).
* `doc_count`     - Documents count.
* `last_id`       - Last used ID.
* `doc_del`       - Documents deleted.
* `av_length`     - Average length of the documents.
* `doc_len_lower` - Statistics about documents length.
* `doc_len_upper` - Statistics about documents length.
* `has_positions` - Boolean indicating if the database has positions stored.
* `shards`        - List of shard databases.
