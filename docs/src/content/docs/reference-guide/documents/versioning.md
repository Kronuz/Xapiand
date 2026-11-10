---
title: "Versioning"
---

Xapiand is concurrent. When documents are created or updated, several requests can
change the same document at once, and a client may write from a copy of the
document that is already out of date. Xapiand needs a way of telling the client
that the document changed under it.

Every document has a `_version` number that is incremented whenever the document
is written. A write that carries no version always applies, and the last write
wins. A write that names the version it expects (the `version` query param, or a
`_version` in the document body) applies only if that is the document's current
version; otherwise it answers `409 Conflict` and changes nothing.

For example, the following indexing command will create a document and assign
a version to it:

```rest
PUT /twitter/user/Kronuz

{
  "name": "Germán Méndez Bravo"
}
```

<!-- e2e:begin
---
description: Versioning: a write assigns a version
---

```js
pm.test("The write answers the document with its version", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal("Kronuz");
  pm.expect(pm.response.json()._version).to.be.a("number");
});
```
e2e:end -->

This is all business as usual. The write answers `204 No Content`; with the
`echo` query param it answers the document, and the interesting bit is its
`_version`:

```json
{
  "name": "Germán Méndez Bravo",
  "_id": "Kronuz",
  "_version": 1
}
```

Notice the `_version` returned when performing an index operation. This is now
the version associated with this particular document. If we get or search for
documents in an index, we will get the version as well.

## Optimistic Concurrency Control

Now, the interesting bits can start, as we can use the versioning feature to
perform [optimistic concurrency control](https://en.wikipedia.org/wiki/Optimistic_concurrency_control).
We can take advantage of the `_version` number to ensure that conflicting
changes made by our application do not result in data loss. We do this by
specifying the version number of the document that we wish to change. If
that version is no longer current, our request fails.

Let’s create a new blog post:

```rest
PUT /blog/1

{
  "title": "My first blog entry",
  "text": "Just trying this out..."
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

The response body tells us that this newly created document has `_version`
number `1`. Now imagine that we want to edit the document: we load its data into
a web form, make our changes, and then save the new version.

First we retrieve the document:

```rest
GET /blog/1
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Optimistic Concurrency Control document", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData["title"]).to.equal("My first blog entry");
  pm.expect(jsonData["text"]).to.equal("Just trying this out...");
});
```
e2e:end -->

The response body includes the same `_version` number of `1`:

```json
{
  "title": "My first blog entry",
  "text": "Just trying this out...",
  "_id": 1,
  "_version": 1
}
```

Now, when we try to save our changes by reindexing the document, we specify the
version to which our changes should be applied. We want this update to succeed
only if the current `_version` of this document in our index is version `1`, so
we pass `version=1` as a query param (or `_version` as part of the document body):

```rest
PUT /blog/1?version=1

{
  "title": "My first blog entry",
  "text": "Starting to get the hang of this..."
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.expect(pm.response.code).to.satisfy(function(code) {
    // Also compare with 409 in case the index with the document exist already with the same version
    return code === 200 || code === 409;
  });
});
```
e2e:end -->

This request succeeds, and the response body tells us that the `_version` has
been incremented to 2:

```json
{
  "title": "My first blog entry",
  "text": "Starting to get the hang of this...",
  "_id": 1,
  "_version": 2
}
```

However, if we were to rerun the same index request, still specifying
version `1`, Xapiand would respond with a `409 Conflict` HTTP response code.
The `409` body carries the document's current version as `_version` (`0` when
there is no document), so a client knows which version to read back.

The version is checked by every write of a document by its ID: `PUT`, `UPDATE`,
`PATCH`, `UPSERT` and `DELETE` (see [Delete](../delete-api#version)). The check
runs in the index's write turn, where the write applies, so two writers that
read the same version never both succeed.

## Creating only when missing

A version of `0` stands for a document that does not exist. `PUT` and `UPSERT`
with `version=0` create the document only when no document has that ID, and
answer `409` when one does; `PATCH`, `UPDATE` and `DELETE` with `version=0` on
a missing document answer `404`, as they do without a version. Any other
version names a document that must be there, so it answers `409` when the
document is missing.

```rest
PUT /versions/doc/1?version=0

{
  "title": "first"
}
```

<!-- e2e:begin
---
description: Versioning: version 0 creates a missing document
---

```js
pm.test("Version 0 creates the document", function () {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /versions/doc/1?version=0

{
  "title": "second"
}
```

<!-- e2e:begin
---
description: Versioning: version 0 is create-only
---

```js
pm.test("Version 0 refuses an existing document", function () {
  pm.response.to.have.status(409);
});
pm.test("The 409 body carries the current version", function () {
  pm.expect(pm.response.json()._version).to.equal(1);
});
```
e2e:end -->

```rest
PUT /versions/doc/1?version=1

{
  "title": "second"
}
```

<!-- e2e:begin
---
description: Versioning: the current version applies
---

```js
pm.test("The current version applies", function () {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
GET /versions/doc/1
```

<!-- e2e:begin
---
description: Versioning: the version moved to 2
---

```js
pm.test("The write moved the version to 2", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._version).to.equal(2);
  pm.expect(pm.response.json().title).to.equal("second");
});
```
e2e:end -->

```rest
PUT /versions/doc/1?version=1

{
  "title": "third"
}
```

<!-- e2e:begin
---
description: Versioning: a stale version
---

```js
pm.test("A stale version answers 409", function () {
  pm.response.to.have.status(409);
});
pm.test("The 409 body carries the current version", function () {
  pm.expect(pm.response.json()._version).to.equal(2);
});
```
e2e:end -->

```rest
PATCH /versions/doc/2?version=0

[
  { "op": "add", "path": "/title", "value": "missing" }
]
```

<!-- e2e:begin
---
description: Versioning: PATCH with version 0 of a missing document
---

```js
pm.test("PATCH with version 0 of a missing document answers 404", function () {
  pm.response.to.have.status(404);
});
```
e2e:end -->

```rest
PUT /versions/doc/2?version=3

{
  "title": "missing"
}
```

<!-- e2e:begin
---
description: Versioning: a version of a missing document
---

```js
pm.test("A version other than 0 of a missing document answers 409", function () {
  pm.response.to.have.status(409);
});
pm.test("The 409 body reports no document", function () {
  pm.expect(pm.response.json()._version).to.equal(0);
});
```
e2e:end -->

Sources: `docs/mechanisms/document-ids-and-versions.md`; `src/database/handler.cc`
(`update`, `patch`: the early check and the `404`); `src/engine/strata/shard.cc`
(`check_version`: a missing document has version 0); `src/server/search_views.cc`
(`on_error` adds `_version` to the `409` body; the `version` query param); `_version`
in the body is read by `src/database/schema.cc`.
