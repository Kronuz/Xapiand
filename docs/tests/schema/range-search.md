---
title: Range Search
---

## Test range search with field_terms

#### Index document with type

{% capture req %}

```json
PUT /test/field_terms/doc20

{
  "foo": {
    "_type": "integer",
    "_value": 20,
    "_index": "field_terms",
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

{% capture req %}

```json
PUT /test/field_terms/doc100

{
  "foo": {
    "_type": "integer",
    "_value": 100,
    "_index": "field_terms",
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

{% capture req %}

```json
PUT /test/field_terms/doc180

{
  "foo": {
    "_type": "integer",
    "_value": 180,
    "_index": "field_terms",
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

{% capture req %}

```json
PUT /test/field_terms/doc210

{
  "foo": {
    "_type": "integer",
    "_value": 210,
    "_index": "field_terms",
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

#### Range search for field_terms

{% capture req %}

```json
SEARCH /test/field_terms/

{
  "_query": {
    "foo": {
      "_in": {
        "_range": {
          "_from": 50,
          "_to": 150
        }
      }
    }
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(3);
  const expected = [20, 100, 180]
  for (var i=0; i < jsonData.hits.length; ++i) {
  	pm.expect(jsonData.hits[i].foo).to.equal(expected[i]);
  }
});
```
---
params: sort=foo
---
{% endcomment %}


## Test range search with field_values

{% capture req %}

```json
PUT /test/field_values/doc20

{
  "foo": {
    "_type": "integer",
    "_value": 20,
    "_index": "field_values",
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

#### Index document with type

{% capture req %}

```json
PUT /test/field_values/doc100

{
  "foo": {
    "_type": "integer",
    "_value": 100,
    "_index": "field_values",
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

{% capture req %}

```json
PUT /test/field_values/doc180

{
  "foo": {
    "_type": "integer",
    "_value": 180,
    "_index": "field_values",
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

{% capture req %}

```json
PUT /test/field_values/doc210

{
  "foo": {
    "_type": "integer",
    "_value": 210,
    "_index": "field_values",
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

#### Range search for field_values

{% capture req %}

```json
SEARCH /test/field_values/

{
  "_query": {
    "foo": {
      "_in": {
        "_range": {
          "_from": 50,
          "_to": 150
        }
      }
    }
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  const expected = [100]
  for (var i=0; i < jsonData.hits.length; ++i) {
    pm.expect(jsonData.hits[i].foo).to.equal(expected[i]);
  }
});
```
---
params: sort=foo
---
{% endcomment %}


## Test range search with field_all

#### Index document with type

{% capture req %}

```json
PUT /test/field_all/doc20

{
  "foo": {
    "_type": "integer",
    "_value": 20,
    "_index": "field_all",
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

{% capture req %}

```json
PUT /test/field_all/doc100

{
  "foo": {
    "_type": "integer",
    "_value": 100,
    "_index": "field_all",
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

{% capture req %}

```json
PUT /test/field_all/doc180

{
  "foo": {
    "_type": "integer",
    "_value": 180,
    "_index": "field_all",
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

{% capture req %}

```json
PUT /test/field_all/doc210

{
  "foo": {
    "_type": "integer",
    "_value": 210,
    "_index": "field_all",
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

####  Range search for field_all

{% capture req %}

```json
SEARCH /test/field_all/

{
  "_query": {
    "foo": {
      "_in": {
        "_range": {
          "_from": 50,
          "_to": 150
        }
      }
    }
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  const expected = [100]
  for (var i=0; i < jsonData.hits.length; ++i) {
    pm.expect(jsonData.hits[i].foo).to.equal(expected[i]);
  }
});
```
---
params: sort=foo
---
{% endcomment %}