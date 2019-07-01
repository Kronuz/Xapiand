---
title: Create Schema
---

## Create Simple Schema

#### Create schema

{% capture req %}

```json
PUT /test/create-schema/

{
  "_schema": {
    "schema": {
      "name": {
        "_type": "text"
      },
      "age": {
        "_type": "positive"
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
{% endcomment %}

#### Schema values are valid

{% capture req %}

```json
GET /test/create-schema/
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
  pm.expect(jsonData._schema.schema.name).to.include({'_type': 'text' });
  pm.expect(jsonData._schema.schema.age).to.include({'_type': 'positive' });
});
```
{% endcomment %}