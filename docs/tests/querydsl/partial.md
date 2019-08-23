---
title: Partial search
---

## Search keywords and text with partial

#### Index keywords values

{% capture req %}

```json
PUT /test/querydsl/partial/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
        "_value": "categorisation"
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

#### Search partial

{% capture req %}

```json
SEARCH /test/querydsl/partial/keyword/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:cate**
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
{% endcomment %}


#### Index text values

{% capture req %}

```json
PUT /test/querydsl/partial/text/doc

{
    "_id": {
        "_type": "text",
    },
    "field": {
        "_type": "text",
        "_value": "dynamic categorisation"
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


#### Search partial

{% capture req %}

```json
SEARCH /test/querydsl/partial/text/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:dynamic%20cate**
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits[0]['_id']).to.equal('doc');
});
```
{% endcomment %}