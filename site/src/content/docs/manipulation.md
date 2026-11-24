---
title: "Data Manipulation"
---

Xapiand reads its own writes: a document you index, update or delete is
visible to the next search or get as soon as the node has answered the write.
The index commits to disk 1 s after its last write (3 s while writes keep
coming, and 8 to 10 s after the first of them at the latest), or before the
answer when the request carries `?commit`. A write the node answered with a 2xx
is in the write-ahead log and survives a crash either way.

Every `PUT`, `POST`, `UPDATE`, `PATCH`, `UPSERT` and `DELETE` below addresses a
document in an index by its path: in `/twitter/user/John` the index is
`twitter/user` and the document ID is `John`. The responses shown are those of
a node that echoes writes (`--echo`, `-vvvv`, or `?echo` on the request); see
[What a write answers](/Xapiand/quickstart#what-a-write-answers) for the
answers without it.

## Indexing/Replacing Documents

We've previously seen how we can index a single document. Let's recall that
command again:

```rest
PUT /twitter/user/John

{
  "name": "John Doe"
}
```

<!-- e2e:begin
---
description: Index: PUT John -> John Doe
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

The above will index the specified document into the `twitter/user` index, with
the ID of `John`. If we then executed the above command again with a different (or same)
document, Xapiand will replace (i.e. reindex) a new document on top of the
existing one with the ID of `John`:

```rest
PUT /twitter/user/John

{
  "name": "John"
}
```

<!-- e2e:begin
---
description: Index: PUT John -> John
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

The above completely overwrites the existent document with the ID of `John` with
the new one.

If, on the other hand, we use a different ID, a new document will be indexed
and the existing document(s) already in the index remains untouched.

```rest
PUT /twitter/user/Jane

{
  "name": "Jane Doe",
  "age": 20
}
```

<!-- e2e:begin
---
description: Index: PUT Jane -> Jane Doe
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

The above indexes a new document with an ID of `Jane`.

When indexing, the ID part is optional. If not specified, Xapiand will generate
an ID and then use it to index the document. The actual ID Xapiand generates
(or whatever we specified explicitly in the previous examples) is returned as
part of the index API call.

This example shows how to index a document without an explicit ID:

```rest
POST /twitter/user/

{
  "name": "Richard Roe"
}
```

<!-- e2e:begin
---
description: Index: POST -> Richard Roe
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

Note that in the above case, we are using the `POST` verb instead of `PUT`
as we're not specifying an explicit ID.

## Updating Documents

In addition to being able to index and replace documents, we can also update
existing documents.

This example shows how to update our previous document (ID of `John`) by adding
a _gender_ field to it and leaving the _name_ intact:

```rest
UPDATE /twitter/user/John

{
  "gender": "male"
}
```

<!-- e2e:begin
---
description: Index: UPDATE John -> male
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Response is merged", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.be.an('string');
  pm.expect(jsonData.gender).to.equal('male');
});
```
e2e:end -->

And the following example shows how to update the same document by adding an
_age_ field to it and at the same time changing the _name_ from simply
_"John"_ to _"Johnny Doe"_:

```rest
UPDATE /twitter/user/John

{
  "name": "Johnny Doe",
  "age": 17
}
```

<!-- e2e:begin
---
description: Index: UPDATE John -> Johnny Doe, 17
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

### Patching Documents

`PATCH` applies a [JSON Patch](https://www.rfc-editor.org/rfc/rfc6902) (an
array of operations) to a document. A body that is not an array of objects
answers `400`. This example creates a document and then replaces one field,
adds another and removes a third:

```rest
PUT /twitter/user/Mary

{
  "name": "Mary Major",
  "age": 31
}
```

```rest
PATCH /twitter/user/Mary

[
  {"op": "replace", "path": "/name", "value": "Mary Minor"},
  {"op": "add", "path": "/city", "value": "Oslo"},
  {"op": "remove", "path": "/age"}
]
```

<!-- e2e:begin
---
description: PATCH Mary -> JSON Patch applied
---

```js
pm.test("The patch is applied", function () {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.equal("Mary Minor");
  pm.expect(jsonData.city).to.equal("Oslo");
  pm.expect(jsonData).to.not.have.property("age");
});
```
e2e:end -->

### Upserting Documents

`UPDATE` of a document that does not exist answers `404 Not Found`. `UPSERT`
merges like `UPDATE` when the document exists and creates it when it does not:

```rest
UPSERT /twitter/user/Nora

{
  "name": "Nora North"
}
```

<!-- e2e:begin
---
description: UPSERT Nora -> created
---

```js
pm.test("UPSERT creates the document", function () {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.equal("Nora North");
  pm.expect(jsonData._id).to.equal("Nora");
  pm.expect(jsonData._version).to.equal(1);
});
```
e2e:end -->

```rest
UPSERT /twitter/user/Nora

{
  "age": 40
}
```

<!-- e2e:begin
---
description: UPSERT Nora -> merged
---

```js
pm.test("UPSERT merges into the existing document", function () {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.equal("Nora North");
  pm.expect(jsonData.age).to.equal(40);
  pm.expect(jsonData._version).to.equal(2);
});
```
e2e:end -->

### Updating With Scripts

Updates can also be performed by using simple scripts. This example uses a
script to increment the _age_ by 5 and at the same time change the _name_:

```rest
UPDATE /twitter/user/John

{
  "name": "John Doe",
  "_script": "_doc.age = _old_doc.age + 5"
}
```

<!-- e2e:begin
---
description: Index: UPDATE John -> John Doe, 22 (scripted)
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Response is merged", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.gender).to.be.an('string');
  pm.expect(jsonData.name).to.equal('John Doe');
});
```

```js
pm.test("Response with script applied", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.age).to.equal(22);
});
```
e2e:end -->

In the above example, `_doc` is the current document and `_old_doc` refers to
the previous document that is about to be updated.

## Deleting Documents

Deleting a document is fairly straightforward. This example shows how to delete
our previous user with the ID of `Jane`:

```rest
DELETE /twitter/user/Jane
```

<!-- e2e:begin
---
description: Index: DELETE Jane
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

Deleting answers `204 No Content`, and `404 Not Found` when the document does
not exist. There is no request that deletes an index; `DELETE /twitter/user/`
answers `501 Not Implemented`:

```rest
DELETE /twitter/user/
```

<!-- e2e:begin
---
description: DELETE of an index is not implemented
---

```js
pm.test("Deleting an index answers 501", function () {
  pm.response.to.have.status(501);
});
```
e2e:end -->

## Checking a Document

`HEAD` answers `200 OK` for a document that exists and `404 Not Found` for one
that does not:

```rest
HEAD /twitter/user/John
```

<!-- e2e:begin
---
description: HEAD of an existing document
---

```js
pm.test("The document exists", function () {
  pm.response.to.have.status(200);
});
```
e2e:end -->

## Committing and Checking an Index

The index commits on its own, as described at the top of this page. `COMMIT`
commits an index now and answers `200 OK` once it is done; the `?commit`
parameter does the same as part of a write:

```rest
COMMIT /twitter/user/
```

<!-- e2e:begin
---
description: COMMIT the twitter/user index
---

```js
pm.test("The index is committed", function () {
  pm.response.to.have.status(200);
});
```
e2e:end -->

`CHECK` verifies an index and reports, for each of its shards, the number of
errors it found (`0` for a sound shard):

```rest
CHECK /twitter/user/
```

<!-- e2e:begin
---
description: CHECK the twitter/user index
---

```js
pm.test("Every shard is sound", function () {
  pm.response.to.be.success;
  var errors = pm.response.json().errors;
  var shards = Object.keys(errors);
  pm.expect(shards.length).to.equal(5);
  shards.forEach(function (shard) {
    pm.expect(errors[shard]).to.equal(0);
  });
});
```
e2e:end -->
