---
title: Casting Values
---

Cast allows explicit conversion from one data type to another as long as types
are compatible.

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "balance" : {
      "_integer": 2221.82
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In the above example cast `2221.46` to integer, which later is internally
converted to `2221.0` (as the "balance" field is a float).

The following example will convert the integer `84535` to text `"84535"`:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "contact.postcode" : {
      "_text": 84535
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .test }

```js
pm.test("response is success", function() {
  pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Casting count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```

{: .test }

```js
pm.test("Casting size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(1);
});
```

{: .test }

```js
pm.test("Casting value is valid", function() {
  var jsonData = pm.response.json();
  var expected = [768];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```


## Type Compatibility

| Types                                 | Compatible Types                                                                        |
|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_integer`                            | `_positive`, `_float`, `_boolean`, `_text`                                              |
| `_positive`                           | `_integer`, `_float`, `_boolean`, `_text`                                               |
| `_float`                              | `_integer`, `_positive`, `_boolean`, `_text`                                            |
| `_boolean`                            | `_integer`, `_positive`, `_float`, `_text`                                              |
| `_text`                               | `_integer`, `_positive`, `_float`, `_boolean`, `_date`, `_time`, `_keyword` and objects |
| `_date` `_time` `_geospatial` `_uuid` | `_text`                                                                                 |
