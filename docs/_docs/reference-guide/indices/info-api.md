---
title: Index Information API
short_title: Index Info API
---

The simplest form of `INFO` gets information about an index:

{% capture req %}

```json
INFO /bank/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
`INFO /bank/` is not the same as `INFO /bank`, the former gets information about
the index `/bank/` while the later retrieves information about a document with
ID `bank` inside index `/`.
[Trailing slashes are important]({{ '/docs/reference-guide/api#resource-paths' | relative_url }}).

The response will include the following information about the index:

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
