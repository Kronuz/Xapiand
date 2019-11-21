---
title: Empty value
---

## Field With Empty Value

#### Index Document With Empty Value

{% capture req %}

```json
PUT /test/empty-field/doc

{
  "field": {
    "_type": "keyword",
    "_index": "field_all"
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
{% endcomment %}

####  Get Document With Empty Value

{% capture req %}

```json
GET /test/empty-field/doc
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._id).to.equal('doc');
});
```
{% endcomment %}