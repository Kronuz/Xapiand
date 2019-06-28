---
title: Set Value With Null
---

## Replace Value by Null

#### Index value


{% capture req %}

```json
PUT /test/replace-null/doc1

{
    "_id": {
        "_type": "keyword"
    },
    "campo": {
        "_type": "integer",
        "_value": 12
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

#### Replace value with null

{% capture req %}

```json
PUT /test/replace-null/doc2

{
    "_id": {
        "_type": "keyword"
    },
    "campo": null
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

#### Get replaced value with null

{% capture req %}

```json
GET /test/replace-null/doc2
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