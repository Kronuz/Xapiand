---
title: Global Terms
---

## Search For Global Term

#### Index for Global

{% capture req %}

```json
PUT /test/doc

{
  "_id": {
    "_type": "keyword",
  },
  "ident": {
    "_type": "uuid",
    "_index": "global_terms",
    "_value": "~notmet"
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

####  Search for Global

{% capture req %}

```json
SEARCH /test/
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
  pm.expect(jsonData.hits[0].ident).to.equal('~notmet');
});
```
{% endcomment %}
