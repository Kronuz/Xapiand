---
title: "Delete Document API"
---

The _Delete API_ deletes a document from a specific index based on its ID. The
following example deletes the document from an index called _"twitter"_ with ID
`1`:

```rest
DELETE /twitter/tweet/1
```

<!-- e2e:begin
---
description: Delete: the twitter document
---

```js
pm.test("DELETE answers 204 No Content", function() {
  pm.response.to.have.status(204);
});
```
e2e:end -->

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

<!-- e2e:begin
---
description: Delete: the document to delete
---

```js
pm.test("The document is written", function() {
  pm.expect(pm.response.code).to.be.oneOf([200, 201, 204]);
});
```
e2e:end -->

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

A `version` query param makes the delete conditional: it applies only when the
document's current version is that number, and otherwise answers `409 Conflict`
and deletes nothing. The `409` body carries the current version as `_version`.
A `version` of `0` names a document that does not exist, so a delete with it
answers `404` (see [Versioning](/Xapiand/reference-guide/documents/versioning)).

```rest
PUT /deleteapi/doc/2

{
  "title": "version 1"
}
```

<!-- e2e:begin
---
description: Delete: the document to delete by version
---

```js
pm.test("The document is written", function() {
  pm.expect(pm.response.code).to.be.oneOf([200, 201, 204]);
});
```
e2e:end -->

```rest
DELETE /deleteapi/doc/2?version=99
```

<!-- e2e:begin
---
description: Delete: a stale version is refused
---

```js
pm.test("DELETE with the wrong version answers 409", function() {
  pm.response.to.have.status(409);
});
pm.test("The 409 body carries the current version", function() {
  pm.expect(pm.response.json()._version).to.equal(1);
});
```
e2e:end -->

```rest
DELETE /deleteapi/doc/2?version=1
```

<!-- e2e:begin
---
description: Delete: the current version deletes
---

```js
pm.test("DELETE with the current version answers 204", function() {
  pm.response.to.have.status(204);
});
```
e2e:end -->

Sources: `src/server/search_views.cc` (`delete_document_view` passes `version`
to `delete_document`) and `docs/mechanisms/document-ids-and-versions.md`.
