---
title: Arrays
---

### Single Array

{% comment %}
```json
PUT /test/array_single/doc

{
  "types": [ "A" ]
}
```
---
description: Index single array
---

```json
GET /test/array_single/._schema.schema.types
```

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
---
description: Get single array
---


```json
INFO /test/array_single/doc
```

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
---
description: Info single array
---
{% endcomment %}


### Simple Array

{% comment %}
```json
PUT /test/array/doc

{
  "types": [
    "A", "B", "C", "D",
    "E", "F", "G", "H"
  ]
}
```
---
description: Index Array
---

```json
GET /test/array/._schema.schema.types
```

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
---
description: Get array
---


```json
INFO /test/array/doc
```

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
---
description: Info array
---
{% endcomment %}


### Array of Array

{% comment %}
```json
PUT /test/array-of-array/doc

{
  "types": [
    [ "A", "B", "C", "D" ],
    [ "E", "F", "G", "H" ]
  ]
}
```
---
description: Index Array of Arrays
---

```json
GET /test/array-of-array/._schema.schema.types
```

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
---
description: Get Array of Arrays
---

```json
INFO /test/array-of-array/doc
```

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
---
description: Info Array of Arrays
---
{% endcomment %}


### Array of Array of Arrays

{% comment %}
```json
PUT /test/array-of-arrays-of-arrays/doc

{
  "types": [
    [ [ "A", "B" ], [ "C", "D" ] ],
    [ [ "E", "F" ], [ "G", "H" ] ]
  ]
}
```
---
description: Index Array of Arrays of Arrays
---

```json
GET /test/array-of-arrays-of-arrays/._schema.schema.types
```

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
---
description: Get Array of Arrays of Arrays
---

```json
INFO /test/array-of-arrays-of-arrays/doc
```

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
---
description: Info Array of Arrays of Arrays
---
{% endcomment %}


### Array Mixed With Text

{% comment %}
```json
PUT /test/arrays/doc

{
  "types": [
    "A", "B", [ "C", "D" ], [ "E", "F", [ "G", "H" ] ],
    "I", [ "J", [ "K", [ "L", [ "M", "N" ]] ] ]
  ]
}
```
---
description: Index Arrays mixed with text
---

```json
GET /test/arrays/._schema.schema.types
```

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
---
description: Get Arrays mixed with text
---

```json
INFO /test/arrays/doc
```

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
---
description: Info Arrays mixed with text
---
{% endcomment %}


### Array of Objects

{% comment %}
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
---
description: Index Array of Objects
---

```json
GET /test/array_of_objects/
```

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
---
description: Get Array of Objects
---

```json
INFO /test/array_of_objects/doc
```

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
---
description: Info Array of Objects
---
{% endcomment %}