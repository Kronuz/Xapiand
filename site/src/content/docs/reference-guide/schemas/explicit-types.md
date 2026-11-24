---
title: "Explicit Types"
---

You know more about your data than Xapiand can guess, so while
[Dynamic Typing](/Xapiand/reference-guide/schemas/dynamic-typing)
is useful to get started, at some point you will want to specify your own
explicit types.

Types are declared inside the `_schema` object when you create an index (or the
first time a field is seen), giving each field a `_type`:

```rest
PUT /test_explicit_types/

{
  "_schema": {
    "name": {
      "_type": "text"
    },
    "age": {
      "_type": "positive"
    }
  }
}
```

Once declared, the type is enforced. Documents that fit the schema are indexed
as usual:

```rest
PUT /test_explicit_types/1

{
  "name": "Jane Austen",
  "age": 41
}
```

And the field is queryable with the declared type:

```rest
SEARCH /test_explicit_types/

{
  "_query": {
    "name": "Jane"
  }
}
```

A value that cannot be coerced into the declared type (for instance a negative
`age` for a `positive` field) is rejected rather than silently reinterpreted.

:::hint[Only for New Fields]{.info}
`_type` can only be set the first time a field is created. Attempting to change
the type of an existing field during an update results in a Bad Request error.
:::

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Explicit-typed field is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
e2e:end -->
