---
title: "Indexing Mode"
---

Indexing mode defines what parts of a document are indexed and how. It is set
per field with the `_index` option and controls two independent dimensions:

* **terms** build the inverted index, so the field can be matched by full-text
  and term queries.
* **values** build document value slots, so the field can be used for sorting,
  ranges, faceting and aggregations.

Each of those can be indexed under the field's own prefix (**field**, so it is
searched by naming the field) and/or under a shared **global** prefix (so it is
searched without naming any field).

Available values for the `_index` option are:

| `_index`          | Terms                  | Values                  |
|-------------------|------------------------|-------------------------|
| `none`            | -                      | -                       |
| `field_terms`     | field                  | -                       |
| `field_values`    | -                      | field                   |
| `field_all`       | field                  | field                   |
| `global_terms`    | global                 | -                       |
| `global_values`   | -                      | global                  |
| `global_all`      | global                 | global                  |
| `terms`           | field + global         | -                       |
| `values`          | -                      | field + global          |
| `all`             | field + global         | field + global          |

`field` is an alias of `field_all` and `global` of `global_all`, and a pair joined by a comma
(such as `"global_terms,field_values"`) combines the terms of one with the values of the other.

The default is `field_all` (both terms and values, under the field prefix). The `_id` field is
always `field_all`. Sources: `src/database/schema.cc:518-690`, `src/database/schema.h:121`.

## Example

The `code` field below is indexed as `field_values` (values only), while `label`
keeps the default `field_all`:

```rest
PUT /test_index_mode/

{
  "_schema": {
    "code": {
      "_type": "keyword",
      "_index": "field_values"
    },
    "label": {
      "_type": "text"
    }
  }
}
```

```rest
PUT /test_index_mode/1

{
  "code": "AB-123",
  "label": "quick brown fox"
}
```

Because `label` has terms, it matches a term query:

```rest
SEARCH /test_index_mode/

{
  "_query": {
    "label": "brown"
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
pm.test("field_all field is term-searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
e2e:end -->

But `code` has only values (no terms), so the same shape of query matches
nothing, even though its value is stored for sorting and ranges:

```rest
SEARCH /test_index_mode/

{
  "_query": {
    "code": "AB-123"
  }
}
```

<!-- e2e:begin

```js
pm.test("field_values field is not term-searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
});
```
e2e:end -->
