---
title: Wildcard search
---

## Search keywords and text with wilcards

#### Index keywords values

{% capture req %}

```json
PUT /test/querydsl/wildcard/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
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

#### Search wildcard

{% capture req %}

```json
SEARCH /test/querydsl/wildcard/keyword/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:dynamic%20cate*
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
PUT /test/querydsl/wildcard/text/doc

{
    "_id": {
        "_type": "keyword",
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


#### Search wildcard

{% capture req %}

```json
SEARCH /test/querydsl/wildcard/text/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:dynamic%20cate*
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
