---
title: "Arrays"
---

## Single Array

#### Index single array

```rest
PUT /test/array_single/doc

{
  "types": [ "A" ]
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get single array

```rest
GET /test/array_single/._schema.types
```

<!-- e2e:begin
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
e2e:end -->

#### Info single array

```rest
INFO /test/array_single/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.include({'1680431078': 'A'});
});
```
e2e:end -->

#### Search single array

```rest
SEARCH /test/array_single/

{
  "_query": {
    "types": "A"
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("The array value is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Simple Array

#### Index Array

```rest
PUT /test/array/doc

{
  "types": [
    "A", "B", "C", "D",
    "E", "F", "G", "H"
  ]
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get array

```rest
GET /test/array/._schema.types
```

<!-- e2e:begin
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
e2e:end -->

#### Info array

```rest
INFO /test/array/doc
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
e2e:end -->

#### Search array

```rest
SEARCH /test/array/

{
  "_query": {
    "_and": [
      {
        "types": "A"
      },
      {
        "types": "B"
      },
      {
        "types": "C"
      },
      {
        "types": "D"
      },
      {
        "types": "E"
      },
      {
        "types": "F"
      },
      {
        "types": "G"
      },
      {
        "types": "H"
      }
    ]
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Every element of the array is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Array of Array

#### Index Array of Arrays

```rest
PUT /test/array-of-array/doc

{
  "types": [
    [ "A", "B", "C", "D" ],
    [ "E", "F", "G", "H" ]
  ]
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

####  Get Array of Arrays

```rest
GET /test/array-of-array/._schema.types
```

<!-- e2e:begin
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
e2e:end -->

#### Info Array of Arrays

```rest
INFO /test/array-of-array/doc
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
e2e:end -->

#### Search Array of Arrays

```rest
SEARCH /test/array-of-array/

{
  "_query": {
    "_and": [
      {
        "types": "A"
      },
      {
        "types": "B"
      },
      {
        "types": "C"
      },
      {
        "types": "D"
      },
      {
        "types": "E"
      },
      {
        "types": "F"
      },
      {
        "types": "G"
      },
      {
        "types": "H"
      }
    ]
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Every element of the nested arrays is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Array of Array of Arrays

####  Index Array of Arrays of Arrays

```rest
PUT /test/array-of-arrays-of-arrays/doc

{
  "types": [
    [ [ "A", "B" ], [ "C", "D" ] ],
    [ [ "E", "F" ], [ "G", "H" ] ]
  ]
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Array of Arrays of Arrays

```rest
GET /test/array-of-arrays-of-arrays/._schema.types
```

<!-- e2e:begin

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
e2e:end -->

#### Info Array of Arrays of Arrays

```rest
INFO /test/array-of-arrays-of-arrays/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H'});
});
```
e2e:end -->

#### Search Array of Arrays of Arrays

```rest
SEARCH /test/array-of-arrays-of-arrays/

{
  "_query": {
    "_and": [
      {
        "types": "A"
      },
      {
        "types": "B"
      },
      {
        "types": "C"
      },
      {
        "types": "D"
      },
      {
        "types": "E"
      },
      {
        "types": "F"
      },
      {
        "types": "G"
      },
      {
        "types": "H"
      }
    ]
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Every element of the deeply nested arrays is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Array Mixed With Text

#### Index Arrays mixed with text

```rest
PUT /test/arrays/doc

{
  "types": [
    "A", "B", [ "C", "D" ], [ "E", "F", [ "G", "H" ] ],
    "I", [ "J", [ "K", [ "L", [ "M", "N" ]] ] ]
  ]
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Arrays mixed with text

```rest
GET /test/arrays/._schema.types
```

<!-- e2e:begin
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
e2e:end -->

#### Info Arrays mixed with text

```rest
INFO /test/arrays/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.include({'1680431078': '\u0000\u0001A\u0001B\u0001C\u0001D\u0001E\u0001F\u0001G\u0001H\u0001I\u0001J\u0001K\u0001L\u0001M\u0001N'});
});
```
e2e:end -->

#### Search Arrays mixed with text

```rest
SEARCH /test/arrays/

{
  "_query": {
    "_and": [
      {
        "types": "A"
      },
      {
        "types": "B"
      },
      {
        "types": "C"
      },
      {
        "types": "D"
      },
      {
        "types": "E"
      },
      {
        "types": "F"
      },
      {
        "types": "G"
      },
      {
        "types": "H"
      },
      {
        "types": "I"
      },
      {
        "types": "J"
      },
      {
        "types": "K"
      },
      {
        "types": "L"
      },
      {
        "types": "M"
      },
      {
        "types": "N"
      }
    ]
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Every element of the mixed arrays is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Array of Objects

#### Index Array of Objects

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Array of Objects

```rest
GET /test/array_of_objects/
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.types._type).to.equal('array/object');
  pm.expect(jsonData._schema.types.property._type).to.equal('text');
  pm.expect(jsonData._schema.types.number._type).to.equal('integer');
});
```
e2e:end -->

#### Info Array of Objects

```rest
INFO /test/array_of_objects/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1666287912','3452157842']);
});
```
e2e:end -->

#### Search Array of Objects

```rest
SEARCH /test/array_of_objects/

{
  "_query": {
    "_and": [
      {
        "types.property": "A"
      },
      {
        "types.property": "B"
      },
      {
        "types.property": "C"
      },
      {
        "types.property": "D"
      },
      {
        "types.property": "E"
      },
      {
        "types.property": "F"
      },
      {
        "types.property": "G"
      },
      {
        "types.property": "H"
      },
      {
        "types.number": 1
      },
      {
        "types.number": 2
      },
      {
        "types.number": 3
      },
      {
        "types.number": 4
      },
      {
        "types.number": 5
      },
      {
        "types.number": 6
      },
      {
        "types.number": 7
      },
      {
        "types.number": 8
      }
    ]
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("The properties and numbers of every object are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->
