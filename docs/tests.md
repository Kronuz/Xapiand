---
title: Tests
---

## Check Datatypes

{% comment %}

---
description: Index Datatypes
---

```json
PUT /test/types/~notmet

{
    "positive": {
        "_type": "positive",
        "_value": 123456
    },
    "integer": {
        "_type": "integer",
        "_value": 123456
    },
    "floating": {
        "_type": "floating",
        "_value": 123456
    },
    "boolean": {
        "_type": "boolean",
        "_value": true
    },
    "keyword": {
        "_type": "keyword",
        "_value": "true"
    },
    "text": {
        "_type": "text",
        "_value": "this field is text"
    },
    "date": {
        "_type": "date",
        "_value": "2019-05-17"
    },
    "datetime": {
        "_type": "datetime",
        "_value": "2019-05-17T10:12:12.123"
    },
    "time": {
        "_type": "time",
        "_value": "10:12:12.123"
    },
    "timedelta": {
        "_type": "timedelta",
        "_value": "+10:12:12.123"
    },
    "uuid": {
        "_type": "uuid",
        "_value": "22214800-78c7-11e9-b7d0-e5256ff63dab"
    }
  }
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```


---
description: Check Datatypes
---

```json
GET /test/types/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```


```js
pm.test("Schema floating type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['floating']._type).to.equal('floating');
});
```

```js
pm.test("Accuracy for floating is valid", function() {
  var jsonData = pm.response.json();
  var expected = [100, 1000, 10000, 100000, 1000000, 100000000];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['floating']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema integer type is valid", function() {
    var jsonData = pm.response.json();
    pm.expect(jsonData._schema.schema['integer']._type).to.equal('integer');
});
```

```js
pm.test("Accuracy for integer is valid", function() {
  var jsonData = pm.response.json();
  var expected = [100, 1000, 10000, 100000, 1000000, 100000000];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['integer']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema positive type is valid", function() {
    var jsonData = pm.response.json();
    pm.expect(jsonData._schema.schema['positive']._type).to.equal('positive');
});
```

```js
pm.test("Accuracy for positive is valid", function() {
  var jsonData = pm.response.json();
  var expected = [100, 1000, 10000, 100000, 1000000, 100000000];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['positive']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema boolean type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['boolean']._type).to.equal('boolean');
});
```


```js
pm.test("Schema keyword type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['keyword']._type).to.equal('keyword');
});
```


```js
pm.test("Schema text type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['text']._type).to.equal('text');
});
```


```js
pm.test("Schema date type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['date']._type).to.equal('date');
});
```

```js
pm.test("Accuracy for date is valid", function() {
  var jsonData = pm.response.json();
  var expected = ['hour', 'day', 'month', 'year', 'decade', 'century'];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['date']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema datetime type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['datetime']._type).to.equal('datetime');
});
```

```js
pm.test("Accuracy for datetime is valid", function() {
  var jsonData = pm.response.json();
  var expected = ['hour', 'day', 'month', 'year', 'decade', 'century'];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['datetime']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema time type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['time']._type).to.equal('time');
});
```

```js
pm.test("Accuracy for time is valid", function() {
  var jsonData = pm.response.json();
  var expected = ['minute', 'hour'];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['time']._accuracy[i]).to.equal(expected[i]);
  }
});
```


```js
pm.test("Schema timedelta type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['timedelta']._type).to.equal('timedelta');
});
```

```js
pm.test("Accuracy for timedelta is valid", function() {
  var jsonData = pm.response.json();
  var expected = ['minute', 'hour'];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData._schema.schema['timedelta']._accuracy[i]).to.equal(expected[i]);
  }
});
```



```js
pm.test("Schema uuid type is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema['uuid']._type).to.equal('uuid');
});
```

{% endcomment %}


{% comment %}

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

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

---
description: Index for Global
---

```json
SEARCH /test/
```

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

---
description: Search for Global
---

{% endcomment %}


{% comment %}

```json
PUT /test/schemas/implicit-_type/doc

{
    "_id": {
        "_type": "keyword",
    },
    "campo": {
        "_type": "keyword",
        "_value": null
    }
}
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
---
description: Index Null
---


```json
GET /test/schemas/implicit-_type/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.campo).to.equal(null);
});
```
---
description: Get Null
---

{% endcomment %}


{% comment %}

```json
PUT /test/replace-null/doc1

{
    "_id": {
        "_type": "keyword"
    },
    "campo": {
        "_type": "integer",
        "_value": 12
    }
}
```
---
description: Index value
---


```json
PUT /test/replace-null/doc2

{
    "_id": {
        "_type": "keyword"
    },
    "campo": null
}
```
---
description: Replace value with null
---


```json
GET /test/replace-null/doc2
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.campo).to.equal(null);
});
```
---
description: Get replaced value with null
---

{% endcomment %}


{% comment %}
```json
POST /test/comment-ignore/

{
  "_recurse": false,
  "_id": {
    "_type": "uuid",
  },
  "#comment": "This comment is ignored",
  "schema": {
    "_type": "object"
  }
}
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  for (key in jsonData.schema) {
      pm.expect(key).to.not.include('#comment');
  }
});
```
---
description: Ignore comment
---

{% endcomment %}


{% comment %}
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
---
description: Create schema
---

```json
GET /test/create-schema/
```

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
---
description: Schema values are valid
---
{% endcomment %}


{% comment %}
```json
PUT /test/create-schema-uuid-field/doc

{
  "_schema": {
    "schema": {
      "_id": {
      "_type": "keyword",
      },
      "<uuid_field>": {
        "meta": {
          "_recurse": false,
        },
        "roles": {
          "_type": "array/keyword",
        }
      }
    }
  },
  "~3pZyPcFqGq": {
    "meta": {
      "scope": "read write"
    },
    "roles": [
      "myself"
    ]
  }
}
```
---
description: Create schema uuid field
---

```json
GET /test/create-schema-uuid-field/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema).to.have.any.keys(['<uuid_field>']);
});
```
---
description: Schema uuid field value is valid
---
{% endcomment %}


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
