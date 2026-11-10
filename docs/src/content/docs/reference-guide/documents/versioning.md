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

The version is checked by `PUT`, `POST`, `UPDATE`, `PATCH` and `UPSERT`. `DELETE` does not
check it (see [Delete](../delete-api#version)).

## Creating only when missing

A version of `0` stands for a document that does not exist: the write applies only
when no document has that ID. Any other version names a document that must be there,
so it also answers `409` when the document is missing.

```rest
PUT /versions/doc/1?version=0

{
  "title": "first"
}
```

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
```
e2e:end -->

```rest
PUT /versions/doc/1?version=1

{
  "title": "second"
}
```

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
```
e2e:end -->

Sources: `src/database/handler.cc:339-342` (the check in `prepare`);
`src/engine/strata/shard.cc:604-611,626-629` (`check_version`: a missing document
has version 0); the `version` query param is read
at `src/server/search_views.cc:2363-2367`, and `_version` in the body at
`src/database/schema.cc:2933`.
