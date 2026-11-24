---
title: "Storing Mode"
---

The `_store` option decides whether a field's value is kept in the stored document. It is a
boolean set per field, and it is independent of [`_index`](indexing-mode): a field with
`"_store": false` is still indexed, so it is searched, sorted and aggregated as usual, but
its value is not in the document that a GET or a search hit returns. Use it for values that
only need to be matched, such as text that is large or private.

The default is `true`.

## Inheritance

A field inherits `_store` from its parent, and a child can only turn it off: a field is
stored when its own `_store` is `true` **and** its parent is stored. Once a field is
`"_store": false`, everything under it stays unstored, even a child that sets
`"_store": true`. Sources: `Schema::feed_store` (`src/database/schema.cc`).

| Parent `_store` | Field `_store` | Field is stored |
|-----------------|----------------|-----------------|
| `true`          | `true`         | yes             |
| `true`          | `false`        | no              |
| `false`         | `true`         | no              |
| `false`         | `false`        | no              |

## Example

The `secret` field below is indexed but not stored. The `profile` object turns `_store` off for
itself, so its `note` field is unstored even though it asks for `"_store": true`; `title`
keeps the default:

```rest
PUT /test_store/

{
  "_schema": {
    "title": {
      "_type": "text"
    },
    "secret": {
      "_type": "text",
      "_store": false
    },
    "profile": {
      "_type": "object",
      "_store": false,
      "note": {
        "_type": "text",
        "_store": true
      }
    }
  }
}
```

```rest
PUT /test_store/1

{
  "title": "visible heading",
  "secret": "hidden passphrase",
  "profile": {
    "note": "private remark"
  }
}
```

### The field matches a search

An unstored field is still indexed, so it matches a query by its name:

```rest
SEARCH /test_store/

{
  "_query": {
    "secret": "passphrase"
  }
}
```

<!-- e2e:begin
---
description: Store: an unstored field matches a search
---

```js
pm.test("The unstored field is searchable", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.equal(1);
});
```
e2e:end -->

The hit carries the stored fields only:

```rest
SEARCH /test_store/

{
  "_query": {
    "secret": "passphrase"
  }
}
```

<!-- e2e:begin
---
description: Store: the search hit lacks the unstored field
---

```js
pm.test("The hit has the stored field and lacks the unstored one", function() {
  var hit = pm.response.json().hits[0];
  pm.expect(hit.title).to.equal("visible heading");
  pm.expect(hit).to.not.have.property("secret");
});
```
e2e:end -->

### The GET body lacks the field

```rest
GET /test_store/1
```

<!-- e2e:begin
---
description: Store: the GET body lacks the unstored field
---

```js
pm.test("The GET body has the stored field and lacks the unstored one", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.title).to.equal("visible heading");
  pm.expect(jsonData).to.not.have.property("secret");
});
```
e2e:end -->

### A child under `_store: false` stays unstored

The `note` field asked for `"_store": true`, but its parent `profile` is unstored, so
the GET body has no `note`:

```rest
GET /test_store/1
```

<!-- e2e:begin
---
description: Store: a child of an unstored parent stays unstored
---

```js
pm.test("The child of an unstored parent is not in the GET body", function() {
  var jsonData = pm.response.json();
  pm.expect(JSON.stringify(jsonData)).to.not.include("private remark");
  pm.expect(JSON.stringify(jsonData)).to.not.include("hidden passphrase");
  if (jsonData.profile) {
    pm.expect(jsonData.profile).to.not.have.property("note");
  }
});
```
e2e:end -->

It is still indexed, so it matches a search by its path:

```rest
SEARCH /test_store/

{
  "_query": {
    "profile.note": "remark"
  }
}
```

<!-- e2e:begin
---
description: Store: a child of an unstored parent is still searchable
---

```js
pm.test("The unstored child is searchable", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
  pm.expect(JSON.stringify(jsonData.hits[0])).to.not.include("private remark");
});
```
e2e:end -->
