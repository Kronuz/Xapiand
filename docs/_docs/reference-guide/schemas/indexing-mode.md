---
title: Indexing Mode
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

The default is `field_all` (both terms and values, under the field prefix).

## Example

The `code` field below is indexed as `field_values` (values only), while `label`
keeps the default `field_all`:

{% capture req %}

```json
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
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
PUT /test_index_mode/1

{
  "code": "AB-123",
  "label": "quick brown fox"
}
```
{% endcapture %}
{% include curl.html req=req %}

Because `label` has terms, it matches a term query:

{% capture req %}

```json
SEARCH /test_index_mode/

{
  "_query": {
    "label": "brown"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

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
{% endcomment %}

But `code` has only values (no terms), so the same shape of query matches
nothing, even though its value is stored for sorting and ranges:

{% capture req %}

```json
SEARCH /test_index_mode/

{
  "_query": {
    "code": "AB-123"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

```js
pm.test("field_values field is not term-searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
});
```
{% endcomment %}
