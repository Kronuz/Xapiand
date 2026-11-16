---
title: "Document Information API"
---

You can retrieve information about a given document ID using the `INFO` method:

```rest
INFO /twitter/tweet/1
```

<!-- e2e:begin
```js
pm.test("returns info for document 1", function () {
  pm.expect(pm.response.json().docid).to.eql(1);
});
```
e2e:end -->

```rest
INFO /twitter/tweet/1?human=true
```

<!-- e2e:begin
---
description: Document info: the version and shard
---

```js
pm.test("The document info has a version and a shard", function () {
  pm.response.to.be.success;
  var info = pm.response.json();
  pm.expect(info.version).to.be.a("number");
  pm.expect(info.shard).to.be.a("number");
  pm.expect(info.data).to.be.an("array");
});
```
e2e:end -->

:::hint{.warning}
`INFO /twitter/tweet/1` is not the same as `INFO /twitter/tweet/1/`, the
former will retrieve information about document `1` inside index `/twitter/tweet/`
and the later will retrieve information about the whole index `/twitter/tweet/1/`.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

The response includes a set of valuable information about the required
document:

* `docid`      - Internal document ID.
* `version`    - The document's version.
* `shard`, `endpoint` - The shard that holds the document, when the index has
  more than one shard.
* `data`       - Content types stored in the document data (`inplace` or `stored`,
  with the volume, offset and size of a stored one; sizes are humanized with
  `human=true`).
* `values`     - Object containing all values stored by the document.
* `raw_data`   - The serialised document data, with the `raw` query param.

Sources: `src/database/handler.cc:1915-1985`, `src/server/search_views.cc:1243-1250`.
