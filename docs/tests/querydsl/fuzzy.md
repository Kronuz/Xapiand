---
title: Fuzzy search
---

## Search keywords and text with partial

#### Index strin values


{% capture req %}

```json
PUT /test/querydsl/fuzzy/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
        "_value": "uncertain"
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

#### Search fuzzy

{% capture req %}

```json
SEARCH /test/querydsl/fuzzy/keyword/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:unserten%7E3
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

#### Index text values

{% capture req %}

```json
PUT /test/querydsl/fuzzy/text/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "text",
        "_value": "uncertain"
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


#### Search fuzzy

{% capture req %}

```json
SEARCH /test/querydsl/fuzzy/text/

```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: query=field:unserten%7E3
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