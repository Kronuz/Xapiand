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

In the above example cast `2221.82` to integer (the fraction is truncated, giving `2221`), which later is internally
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
| `_text`                               | `_integer`, `_positive`, `_float`, `_boolean`, `_datetime`, `_keyword` and objects      |
| `_datetime`, `_uuid`, the geospatial words | `_text`                                                                            |

## Cast words

The query DSL accepts these cast words: `_float`, `_positive`, `_integer`, `_boolean`,
`_keyword` (and its legacy spellings `_term` and `_string`), `_text`, `_datetime`, `_uuid`, and the
geospatial ones, `_ewkt`, `_point`, `_circle`, `_polygon`, `_chull`, `_multipoint`,
`_multicircle`, `_multipolygon`, `_multichull`, `_multiconvex`, `_geometrycollection` and
`_geometryintersection`. A word that is not in the list, such as `_date`, `_time`,
`_timedelta` or `_geospatial`, answers `400` ("Invalid operator"): a date is cast with
`_datetime`.

```rest
SEARCH /bank/

{
  "_query": {
    "age": {
      "_date": "2020-01-01"
    }
  }
}
```

<!-- e2e:begin
---
description: Casting: _date is not a cast word
---

```js
pm.test("_date is not a cast word", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("Invalid operator");
});
```
e2e:end -->

Sources: `src/query_dsl.cc:255-276,278-386`, `src/reserved/types.h:28-53`; the truncation, `src/cast.cc:156-164`.
