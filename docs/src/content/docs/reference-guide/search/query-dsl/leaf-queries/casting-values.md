---
title: "Casting Values"
---

Cast allows explicit conversion from one data type to another as long as types
are compatible.

```rest
SEARCH /bank/

{
  "_query": {
    "balance" : {
      "_integer": 2221.82
    }
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Casting Values results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
  pm.expect(jsonData.hits.length).to.equal(1);
});
```
e2e:end -->

In the above example cast `2221.46` to integer, which later is internally
converted to `2221.0` (as the "balance" field is a float).

The following example will convert the integer `84535` to text `"84535"`:

```rest
SEARCH /bank/

{
  "_query": {
    "contact.postcode" : {
      "_text": 84535
    }
  }
}
```

<!-- e2e:begin
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Casting count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```

```js
pm.test("Casting size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(1);
});
```

```js
pm.test("Casting value is valid", function() {
  var jsonData = pm.response.json();
  var expected = [768];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

## Type Compatibility

| Types                                 | Compatible Types                                                                        |
|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_integer`                            | `_positive`, `_float`, `_boolean`, `_text`                                              |
| `_positive`                           | `_integer`, `_float`, `_boolean`, `_text`                                               |
| `_float`                              | `_integer`, `_positive`, `_boolean`, `_text`                                            |
| `_boolean`                            | `_integer`, `_positive`, `_float`, `_text`                                              |
| `_text`                               | `_integer`, `_positive`, `_float`, `_boolean`, `_date`, `_time`, `_keyword` and objects |
| `_date` `_time` `_geospatial` `_uuid` | `_text`                                                                                 |
