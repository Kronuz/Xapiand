---
title: "Casting Types"
---

Casting explicitly converts a value from one data type to another during
indexing, as long as the types are compatible. It is the indexing-side
counterpart of
[Casting Values](/Xapiand/reference-guide/search/query-dsl/leaf-queries/casting-values),
which does the same at search time.

A cast is written as an object whose single key is the target type. For example,
given an `integer` field, a value that arrives as a float can be cast so it is
stored and indexed as an integer:

```rest
PUT /test_casting_types/

{
  "_schema": {
    "total": {
      "_type": "integer"
    }
  }
}
```

```rest
PUT /test_casting_types/1

{
  "total": {
    "_integer": 2221.82
  }
}
```

The float `2221.82` is cast to the integer `2221`, so the document matches an
integer query:

```rest
SEARCH /test_casting_types/

{
  "_query": {
    "total": 2221
  }
}
```

See
[Casting Values](/Xapiand/reference-guide/search/query-dsl/leaf-queries/casting-values#type-compatibility)
for the full table of compatible types.

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Cast value is indexed as target type", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
e2e:end -->
