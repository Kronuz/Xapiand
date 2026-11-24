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
converted to `2221.0` (as the "balance" field is a float). A cast is the one way to convert a value: without it, a value
must already be exactly of the field's type (see [Exact Values](/Xapiand/reference-guide/schemas/field-types/numeric-type#exact-values)).
A cast truncates a fraction but never wraps: a value outside the type's range, such as `{"_positive": -1}`, answers `400`.

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
`_keyword` (and its legacy spellings `_term` and `_string`), `_text`, `_datetime`, `_date`, `_time`,
`_timedelta`, `_uuid`, and the geospatial ones, `_ewkt`, `_point`, `_circle`, `_polygon`, `_chull`,
`_multipoint`, `_multicircle`, `_multipolygon`, `_multichull`, `_multiconvex`,
`_geometrycollection` and `_geometryintersection`. A word that is not in the list, such as
`_geospatial`, answers `400` ("Invalid operator").

`_date`, `_time` and `_timedelta` find the documents whose field holds that date, time or time
delta, on a field of that type or with no field at all (the values indexed globally). Globally a
time and a time delta share one term space, so `_time` `10:30:00` also finds a `+10:30:00` time
delta, and the reverse. A `_date`
is cast as a `_datetime`. A `_datetime` can also be spelled as a map of `_date` and `_time`, where
the `_date` is a map of `_year`, `_month` and `_day`; a `_date` that is text there answers `400`
("'_date' must be a map value"), and a value that is not a date answers `400` ("Error format in
_datetime").

```rest
SEARCH /bank/

{
  "_query": {
    "joined": {
      "_date": "2020-01-01"
    }
  }
}
```

<!-- e2e:begin
---
description: Casting: _date is a cast word
---

```js
pm.test("_date is a cast word", function() {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json().hits).to.have.lengthOf(0);
});
```
e2e:end -->

Sources: `QueryDSL::process` in `src/query_dsl.cc`, `src/reserved/types.h:28-53`; the truncation and the range checks, `Cast::integer` and `Cast::positive` in `src/cast.cc`.
