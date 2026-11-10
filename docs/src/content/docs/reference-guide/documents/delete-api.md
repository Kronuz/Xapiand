---
title: "Delete Document API"
---

The _Delete API_ deletes a document from a specific index based on its ID. The
following example deletes the document from an index called _"twitter"_ with ID
`1`:

```rest
DELETE /twitter/tweet/1
```

The result of the above delete operation is a `204 No Content` HTTP response code
with no body. Deleting a document that is not there answers `404 Not Found`.

:::hint{.warning}
`DELETE /twitter/tweet/1` is not the same as `DELETE /twitter/tweet/1/`: the
former deletes document `1` inside index `/twitter/tweet/`, and the latter
names the _index_ `/twitter/tweet/1/`. Xapiand does not delete indexes over
HTTP, so `DELETE` on an index path answers `501 Not Implemented`
(see [Delete Index](/Xapiand/reference-guide/indexes/delete-api)).
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

A delete takes effect for the next search or get right away (reads see the
writes of the node, see [Visibility and Durability](/Xapiand/basics#visibility-and-durability)):

```rest
PUT /deleteapi/doc/1

{
  "title": "to be deleted"
}
```

```rest
DELETE /deleteapi/doc/1
```

<!-- e2e:begin
---
description: Delete: DELETE answers 204
---

```js
pm.test("DELETE answers 204 No Content", function() {
  pm.response.to.have.status(204);
});
```
e2e:end -->

```rest
GET /deleteapi/doc/1
```

<!-- e2e:begin
---
description: Delete: the document is gone
---

```js
pm.test("The deleted document is gone", function() {
  pm.response.to.have.status(404);
});
```
e2e:end -->

```rest
DELETE /deleteapi/doc/1
```

<!-- e2e:begin
---
description: Delete: deleting it again
---

```js
pm.test("A second DELETE answers 404", function() {
  pm.response.to.have.status(404);
});
```
e2e:end -->

## Commit

By passing the `commit` query param to the request, you can ensure the operation
returns once the delete has been committed to disk.

:::hint{.caution}
Try limiting the use of `commit` as it will hit performance.
:::

## Version

`DELETE` does not check the document's version: a `version` query param is
read and not compared, so the delete is never refused with `409`. Use
[`PUT` or `UPDATE` with `version`](/Xapiand/reference-guide/documents/versioning)
for compare-and-set writes.

```rest
PUT /deleteapi/doc/2

{
  "title": "version 1"
}
```

```rest
DELETE /deleteapi/doc/2?version=99
```

<!-- e2e:begin
---
description: Delete: the version is not checked
---

```js
pm.test("DELETE ignores the version", function() {
  pm.response.to.have.status(204);
});
```
e2e:end -->

Sources: `src/server/search_views.cc:833-851` (`delete_document_view` reads
`version` and does not pass it on) and `src/database/shard.cc:1448-1462`.
