---
title: "Index Information API"
---

The simplest form of `INFO` gets information about an index:

```rest
INFO /twitter/tweet/
```

<!-- e2e:begin
```js
pm.test("returns info for the index", function () {
  var body = pm.response.json();
  pm.expect(body.endpoint).to.eql("twitter/tweet");
  pm.expect(body).to.have.property("doc_count");
});
```
e2e:end -->

:::hint{.warning}
`INFO /twitter/tweet/1` is not the same as `INFO /twitter/tweet/1/`, the
former will retrieve information about document `1` inside index `/twitter/tweet/`
and the later will retrieve information about thewhole index `/twitter/tweet/1/`.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

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
