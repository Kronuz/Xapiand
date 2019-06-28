---
title: Arrays
---

## Single Array


#### Index single array

{% capture req %}

```json
PUT /test/array_single/doc

{
  "types": [ "A" ]
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

#### Get single array

{% capture req %}

```json
GET /test/array_single/._schema.schema.types
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
  pm.expect(jsonData._type).to.equal('array/text');
});
```
{% endcomment %}

#### Info single array

{% capture req %}

```json
INFO /test/array_single/doc
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
  pm.expect(jsonData.terms.types).to.have.any.keys(['Sa']);
  pm.expect(jsonData.values).to.include({'1680431078': 'A'});
});
```
{% endcomment %}


## Simple Array


#### Index Array

{% capture req %}

```json
PUT /test/array/doc

{
  "types": [
    "A", "B", "C", "D",
    "E", "F", "G", "H"
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

#### Get array

{% capture req %}

```json
GET /test/array/._schema.schema.types
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
  pm.expect(jsonData._type).to.equal('array/text');
});
```
{% endcomment %}

#### Info array

{% capture req %}

```json
INFO /test/array/doc
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
  pm.expect(jsonData.terms.types).to.have.all.keys(['Sa','Sb','Sc','Sd','Se','Sf','Sg','Sh']);
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
{% endcomment %}


## Array of Array


#### Index Array of Arrays

{% capture req %}

```json
PUT /test/array-of-array/doc

{
  "types": [
    [ "A", "B", "C", "D" ],
    [ "E", "F", "G", "H" ]
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

####  Get Array of Arrays

```json
GET /test/array-of-array/._schema.schema.types
```

{% comment %}
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._type).to.equal('array/text');
});
```
{% endcomment %}

#### Info Array of Arrays

{% capture req %}

```json
INFO /test/array-of-array/doc
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
  pm.expect(jsonData.terms.types).to.have.all.keys(['Sa','Sb','Sc','Sd','Se','Sf','Sg','Sh']);
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
{% endcomment %}


## Array of Array of Arrays


####  Index Array of Arrays of Arrays

{% capture req %}

```json
PUT /test/array-of-arrays-of-arrays/doc

{
  "types": [
    [ [ "A", "B" ], [ "C", "D" ] ],
    [ [ "E", "F" ], [ "G", "H" ] ]
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

#### Get Array of Arrays of Arrays

{% capture req %}

```json
GET /test/array-of-arrays-of-arrays/._schema.schema.types
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
  pm.expect(jsonData._type).to.equal('array/text');
});
```
{% endcomment %}

#### Info Array of Arrays of Arrays

{% capture req %}

```json
INFO /test/array-of-arrays-of-arrays/doc
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
  pm.expect(jsonData.terms.types).to.have.all.keys(['Sa','Sb','Sc','Sd','Se','Sf','Sg','Sh']);
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
{% endcomment %}


## Array Mixed With Text


#### Index Arrays mixed with text

{% capture req %}

```json
PUT /test/arrays/doc

{
  "types": [
    "A", "B", [ "C", "D" ], [ "E", "F", [ "G", "H" ] ],
    "I", [ "J", [ "K", [ "L", [ "M", "N" ]] ] ]
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

#### Get Arrays mixed with text

{% capture req %}

```json
GET /test/arrays/._schema.schema.types
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
  pm.expect(jsonData._type).to.equal('array/text');
});
```
{% endcomment %}

#### Info Arrays mixed with text

{% capture req %}

```json
INFO /test/arrays/doc
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
  pm.expect(jsonData.terms.types).to.have.all.keys(['Sa','Sb','Sc','Sd','Se','Sf','Sg','Sh','Si','Sj','Sk','Sl','Sm','Sn']);
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H\u0001I\u0001J\u0001K\u0001L\u0001M\u0001N'});
});
```
{% endcomment %}


## Array of Objects

#### Index Array of Objects

{% capture req %}

```json
PUT /test/array_of_objects/doc

{
  "types": [
    {
      "property": "A",
      "number": 1
    },
    {
      "property": "B",
      "number": 2
    },
    {
      "property": "C",
      "number": 3
    },
    {
      "property": "D",
      "number": 4
    },
    {
      "property": "E",
      "number": 5
    },
    {
      "property": "F",
      "number": 6
    },
    {
      "property": "G",
      "number": 7
    },
    {
      "property": "H",
      "number": 8
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

#### Get Array of Objects

{% capture req %}

```json
GET /test/array_of_objects/
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
  pm.expect(jsonData._schema.schema.types._type).to.equal('array/object');
  pm.expect(jsonData._schema.schema.types.property._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.number._type).to.equal('integer');
});
```
{% endcomment %}

#### Info Array of Objects

{% capture req %}

```json
INFO /test/array_of_objects/doc
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
  pm.expect(jsonData.terms.types.property).to.have.all.keys(['Sa','Sb','Sc','Sd','Se','Sf','Sg','Sh']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1666287912','3452157842']);
});
```
{% endcomment %}