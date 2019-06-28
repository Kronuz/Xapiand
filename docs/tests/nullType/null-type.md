---
title: Null type
---

## Check Null type

#### Index Null

{% capture req %}

```json
PUT /test/schemas/implicit-_type/doc

{
    "_id": {
        "_type": "keyword",
    },
    "campo": {
        "_type": "keyword",
        "_value": null
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

#### Get Null

{% capture req %}

```json
GET /test/schemas/implicit-_type/doc
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
  pm.expect(jsonData.campo).to.equal(null);
});
```
{% endcomment %}
