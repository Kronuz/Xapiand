---
title: Array Objects
---

## Create Schema With Array Objects

#### Create Schema With Document

{% capture req %}

```json
PUT /test/object-array/~notmet

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  },
  "_schema": {
    "foo": {
      "_type": "array/object",
      "_recurse": false,
      "_index": "none"
    }
  },
  "foo": [
    {
      "color": "red",
      "size": 123
    }
  ]
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

#### Get schema

{% capture req %}

```json
GET /test/object-array/
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
  pm.expect(jsonData._schema.foo._type).to.equal('array/object');
});
```
{% endcomment %}


#### Create Schema with Document and Incomplete Type

{% capture req %}

```json
PUT /test/object-array-incomplete/~notmet

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  },
  "_schema": {
    "foo": {
      "_type": "object",
      "_recurse": false,
      "_index": "none"
    }
  },
  "foo": [
    {
      "color": "red",
      "size": 123
    }
  ]
}
```
{% endcapture %}
{% include curl.html req=req %}


{% comment %}
```js
pm.test("Response is success", function() {
  pm.expect(pm.response.code).to.equal(412);
});
```
{% endcomment %}