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


### Global terms

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


### Null type


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


### Replace Value by Null

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


### Ignore Comments

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


### Create Schema

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


### Create Schema With uuid Field

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


### Array

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


### Object of Mixed Types

{% comment %}
```json
PUT /test/mixed_objects/doc

{
  "types": {
    "type": "human",
    "legs": 2,
    "arms": 2,
  }
}
```
---
description: Index Mixed Objects
---

```json
GET /test/mixed_objects/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.arms._type).to.equal('integer');
});
```
---
description: Get Mixed Objects
---

```json
INFO /test/mixed_objects/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.types.arms).to.have.all.keys(['#186a0', '#2710', '#3e8', '#5f5e100', '#64', '#f4240', 'N'+ String.fromCharCode(65533) +'@']);
  pm.expect(jsonData.terms.types.legs).to.have.all.keys(['#186a0', '#2710', '#3e8', '#5f5e100', '#64', '#f4240', 'N'+ String.fromCharCode(65533) +'@']);
  pm.expect(jsonData.terms.types.type).to.have.all.keys(['Shuman']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1663382011','3248593248', '3741895486']);
});
```
---
description: Info Mixed Objects
---
{% endcomment %}


### Value With Nested Object

{% comment %}
```json
PUT /test/value_object_nested/doc

{
  "types": {
    "_value": {
      "type": "human",
      "legs": 2,
      "arms": 2,
      "name": {
        "first": "John",
        "last": "Doe"
      }
    }
  }
}
```
---
description: Index value with nested object
---

```json
GET /test/value_object_nested/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.arms._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.name._type).to.equal('object');
  pm.expect(jsonData._schema.schema.types.name.first._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.name.last._type).to.equal('text');
});
```

{% endcomment %}


### Object

{% comment %}
```json
PUT /test/object/doc

{
  "name": {
    "_value": {
      "first": "John",
      "middle": "R",
      "last": "Doe"
    }
  }
}
```
---
description: Index Object
---

```json
GET /test/object/._schema.schema.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._type).to.equal('object');
});
```
---
description: Get Object
---

```json
INFO /test/object/doc.terms.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.first).to.have.all.keys(['Sjohn']);
  pm.expect(jsonData.last).to.have.all.keys(['Sdoe']);
  pm.expect(jsonData.middle).to.have.all.keys(['Sr']);
});
```
---
description: Info Object
---
{% endcomment %}


### Nested Object

{% comment %}
```json
PUT /test/nested_object/doc

{
  "name": {
    "_value": {
      "first": "John",
      "middle": "R",
      "last": "Doe"
    }
  }
}
```
---
description: Index Nested Object
---

```json
GET /test/nested_object/._schema.schema.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._type).to.equal('object');
});
```
---
description: Get Nested Object
---

```json
INFO /test/nested_object/doc.terms.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.first).to.have.all.keys(['Sjohn']);
  pm.expect(jsonData.last).to.have.all.keys(['Sdoe']);
  pm.expect(jsonData.middle).to.have.all.keys(['Sr']);
});
```
---
description: Info Nested Object
---
{% endcomment %}


### Complex Object

{% comment %}
```json
PUT /test/complex_object/doc

{
  "accountNumber": 121931,
  "balance": 221.46,
  "employer": "Globoil",
  "name": {
    "firstName": "Michael",
    "lastName": "Lee"
  },
  "age": 24,
  "gender": "male",
  "contact": {
    "address": "630 Victor Road",
    "city": "Leyner",
    "state": "Indiana",
    "postcode": "61952",
    "phone": "+1 (924) 594-3216",
    "email": "michael.lee@globoil.co.uk"
  },
  "checkin": {
    "_point": {
      "_longitude": -95.63079,
      "_latitude": 31.76212
    }
  },
  "favoriteFruit": "lemon",
  "eyeColor": "blue",
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": "khakis",
      "shirt": "t-shirt"
    },
    "hairstyle": "slick back"
  },
  "personality": {
    "_language": "en",
    "_value": "A lot can be assumed when you first see Michael Lee, but at the very least you will find out he is elegant and heroic. Of course he is also loyal, passionate and clever, but in a way they are lesser traits and tained by behaviors of being prejudiced as well. His elegance though, this is what he is kind of cherished for. Friends frequently count on this and his excitement in times of need. All in all, Michael has a range of flaws to deal with too. His disruptive nature and insulting nature risk ruining pleasant moods and reach all around. Fortunately his heroic nature helps lighten the blows and moods when needed.",
    "_type": "text"
  }
}
```
---
description: Index Complex Object
---

```json
GET /test/complex_object/._schema.schema
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.accountNumber._type).to.equal('integer');
  pm.expect(jsonData.balance._type).to.equal('floating');
  pm.expect(jsonData.employer._type).to.equal('text');
  pm.expect(jsonData.name._type).to.equal('object');
  pm.expect(jsonData.name.firstName._type).to.equal('text');
  pm.expect(jsonData.name.lastName._type).to.equal('text');
  pm.expect(jsonData.age._type).to.equal('integer');
  pm.expect(jsonData.gender._type).to.equal('text');
  pm.expect(jsonData.contact._type).to.equal('object');
  pm.expect(jsonData.contact.address._type).to.equal('text');
  pm.expect(jsonData.contact.city._type).to.equal('text');
  pm.expect(jsonData.contact.state._type).to.equal('text');
  pm.expect(jsonData.contact.postcode._type).to.equal('text');
  pm.expect(jsonData.contact.phone._type).to.equal('text');
  pm.expect(jsonData.contact.email._type).to.equal('text');
  pm.expect(jsonData.checkin._type).to.equal('geo');
  pm.expect(jsonData.favoriteFruit._type).to.equal('text');
  pm.expect(jsonData.eyeColor._type).to.equal('text');
  pm.expect(jsonData.style._type).to.equal('text');
  pm.expect(jsonData.personality._type).to.equal('text');
});
```
---
description: Get Complex Object
---

```json
INFO /test/complex_object/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
  var jsonData = pm.response.json();

  pm.expect(jsonData.terms).to.have.any.keys(['Zpersonality']);
  pm.expect(jsonData.terms['Zpersonality']).to.have.all.keys(['Sassum','Sbehavior', 'Sblow', 'Scherish', 'Sclever', 'Scount', 'Sdeal', 'Sdisrupt', 'Seleg', 'Sexcit', 'Sfind', 'Sflaw', 'Sfortun', 'Sfrequent', 'Sfriend', 'Shelp', 'Sheroic', 'Sinsult', 'Skind', 'Slee', 'Slesser', 'Slighten', 'Slot', 'Sloyal', 'Smichael', 'Smood', 'Snatur', 'Sneed', 'Spassion', 'Spleasant', 'Sprejud', 'Srang', 'Sreach', 'Srisk', 'Sruin', 'Stain', 'Stime', 'Strait']);
  pm.expect(jsonData.terms['contact']['address']).to.have.all.keys(['S630','Sroad', 'Svictor']);
  pm.expect(jsonData.terms['contact']['city']).to.have.all.keys(['Sleyner']);
  pm.expect(jsonData.terms['contact']['email']).to.have.all.keys(['Sco', 'Sgloboil', 'Slee', 'Smichael', 'Suk']);
  pm.expect(jsonData.terms['contact']['phone']).to.have.all.keys(['S1', 'S3216', 'S594', 'S924']);
  pm.expect(jsonData.terms['contact']['postcode']).to.have.all.keys(['S61952']);
  pm.expect(jsonData.terms['contact']['state']).to.have.all.keys(['Sindiana']);
  pm.expect(jsonData.terms['employer']).to.have.all.keys(['Sgloboil']);
  pm.expect(jsonData.terms['eyeColor']).to.have.all.keys(['Sblue']);
  pm.expect(jsonData.terms['favoriteFruit']).to.have.all.keys(['Slemon']);
  pm.expect(jsonData.terms['gender']).to.have.all.keys(['Smale']);
  pm.expect(jsonData.terms['name']['firstName']).to.have.all.keys(['Smichael']);
  pm.expect(jsonData.terms['name']['lastName']).to.have.all.keys(['Slee']);
  pm.expect(jsonData.terms['personality']).to.have.all.keys(['Sa', 'Sall', 'Salso', 'Sand', 'Sare', 'Saround', 'Sas', 'Sassumed', 'Sat', 'Sbe', 'Sbehaviors', 'Sbeing', 'Sblows', 'Sbut', 'Sby', 'Scan', 'Scherished', 'Sclever', 'Scount', 'Scourse', 'Sdeal', 'Sdisruptive', 'Selegance', 'Selegant', 'Sexcitement', 'Sfind', 'Sfirst', 'Sflaws', 'Sfor', 'Sfortunately', 'Sfrequently', 'Sfriends', 'Shas', 'She', 'Shelps', 'Sheroic', 'Shis', 'Sin', 'Sinsulting', 'Sis', 'Skind', 'Sleast', 'Slee', 'Slesser', 'Slighten', 'Slot', 'Sloyal', 'Smichael', 'Smoods', 'Snature', 'Sneed', 'Sneeded', 'Sof', 'Son', 'Sout', 'Spassionate', 'Spleasant', 'Sprejudiced', 'Srange', 'Sreach', 'Srisk', 'Sruining', 'Ssee', 'Stained', 'Sthe', 'Sthey', 'Sthis', 'Sthough', 'Stimes', 'Sto', 'Stoo', 'Straits', 'Svery', 'Sway', 'Swell', 'Swhat', 'Swhen', 'Swill', 'Swith', 'Syou']);
  pm.expect(jsonData.terms['style']['clothing']['pants']).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms['style']['clothing']['shirt']).to.have.all.keys(['Sshirt', 'St']);
  pm.expect(jsonData.terms['style']['hairstyle']).to.have.all.keys(['Sback', 'Sslick']);
  pm.expect(jsonData.terms['style']['pants']).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms['style']['shirt']).to.have.all.keys(['Sshirt', 'St']);

  var t1 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(97) + String.fromCharCode(65533);
  var t2 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(117) + String.fromCharCode(48);
  var t3 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(118) + String.fromCharCode(42);
  var t4 = String.fromCharCode(78) + String.fromCharCode(65533);
  var t5 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(119) + String.fromCharCode(11);
  var t6 = String.fromCharCode(78) + String.fromCharCode(65533);
  var t7 = String.fromCharCode(78)+ String.fromCharCode(65533) + 'w' + '\u0012' + String.fromCharCode(65533);

  pm.expect(jsonData.terms.accountNumber).to.have.all.keys(['#186a0', '#2710', '#3e8', '#5f5e100', '#64', '#f4240', t7]);
  pm.expect(jsonData.terms.accountNumber['#186a0']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.accountNumber['#2710']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.accountNumber['#3e8']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.accountNumber['#5f5e100']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.accountNumber['#64']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.accountNumber['#f4240']).to.have.any.keys([t6]);

  t1 = String.fromCharCode(78) + String.fromCharCode(65533);
  t2 = String.fromCharCode(78) + String.fromCharCode(65533);
  t3 = String.fromCharCode(78) + String.fromCharCode(65533);
  t4 = String.fromCharCode(78) + String.fromCharCode(65533);
  t5 = String.fromCharCode(78) + String.fromCharCode(65533);
  t6 = String.fromCharCode(78) + String.fromCharCode(65533);
  t7 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(32);

  pm.expect(jsonData.terms.age).to.have.all.keys(['#186a0', '#2710', '#3e8', '#5f5e100', '#64', '#f4240', t7]);
  pm.expect(jsonData.terms.age['#186a0']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.age['#2710']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.age['#3e8']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.age['#5f5e100']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.age['#64']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.age['#f4240']).to.have.any.keys([t6]);

  t1 = String.fromCharCode(78) + String.fromCharCode(65533);
  t2 = String.fromCharCode(78) + String.fromCharCode(65533);
  t3 = String.fromCharCode(78) + String.fromCharCode(65533);
  t4 = String.fromCharCode(78) + String.fromCharCode(65533);
  t5 = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(36);
  t6 = String.fromCharCode(78) + String.fromCharCode(65533);
  t7 = String.fromCharCode(78) + String.fromCharCode(65533);
  var t8 = String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(20) + String.fromCharCode(124);

  pm.expect(jsonData.terms.balance).to.have.all.keys(['#186a0', '#2710', '#3e8', '#5f5e100', '#64', '#f4240', t7]);
  pm.expect(jsonData.terms.balance['#186a0']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.balance['#2710']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.balance['#3e8']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.balance['#5f5e100']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.balance['#64']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.balance['#f4240']).to.have.any.keys([t6]);
  pm.expect(jsonData.terms.balance[t7]).to.have.any.keys([t8]);

  t1 = String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(65533);
  t2 = String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(65533);
  t3 = String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(8) + String.fromCharCode(65533) + String.fromCharCode(64);
  t4 = String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(8) + String.fromCharCode(65533) + String.fromCharCode(71);
  t5 = String.fromCharCode(71) + String.fromCharCode(65533) + String.fromCharCode(8) + String.fromCharCode(65533) + String.fromCharCode(71) + String.fromCharCode(40);
  t6 = String.fromCharCode(71) + String.fromCharCode(392) + String.fromCharCode(65533) + String.fromCharCode(71) + String.fromCharCode(40) + String.fromCharCode(96);

  pm.expect(jsonData.terms.checkin).to.have.all.keys(['#3', '#5', '#8', '#a', '#c', '#f', 'G\u0000']);
  pm.expect(jsonData.terms.checkin['#3']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.checkin['#5']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.checkin['#8']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.checkin['#a']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.checkin['#c']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.checkin['#f']).to.have.any.keys([t6]);

});
```
---
description: Info Complex Object
---
{% endcomment %}


### Reserved Subfield

{% comment %}
```json
PUT /test/reserved_subfield/doc

{
  "name": {
    "_type": "text",
    "_reserved": "this is reserved, should fail"
  }
}
```
---
description: Index Misuse Reserved Subfield
---

```js
pm.test("Response is success", function() {
  pm.expect(pm.response.code).to.equal(400);
});
```
{% endcomment %}


### Namespace

{% comment %}
```json
PUT /test/namespace/doc

{
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": "khakis",
      "shirt": "t-shirt"
    },
    "hairstyle": "slick back"
  }
}
```
---
description: Index Namespace
---

```json
GET /test/namespace/._schema.schema.style
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._namespace).to.equal(true);
});
```
---
description: Get Namespace
---

```json
INFO /test/namespace/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.style.clothing.pants).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms.style.clothing.shirt).to.have.all.keys(['Sshirt', 'St']);
  pm.expect(jsonData.terms.style.hairstyle).to.have.all.keys(['Sback', 'Sslick']);
  pm.expect(jsonData.terms.style.pants).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms.style.shirt).to.have.all.keys(['Sshirt', 'St']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1']);
});
```
---
description: Info Namespace
---
{% endcomment %}


### Strict Namespace

{% comment %}
```json
PUT /test/strict_namespace/doc

{
  "_strict": true,
  "tags": {
    "_namespace": true,
    "_type": "keyword",
    "field": {
      "subfield": {
        "_value": "value",
      }
    }
  }
}
```
---
description: Index Strict Namespace
---

```json
GET /test/strict_namespace/._schema.schema.tags
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._type).to.equal('keyword');
});
```
---
description: Get Strict Namespace
---

```json
INFO /test/strict_namespace/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.tags.field.subfield).to.have.any.keys(['Kvalue']);
  pm.expect(jsonData.terms.tags.subfield).to.have.any.keys(['Kvalue']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1']);
});
```
---
description: Info Strict Namespace
---
{% endcomment %}


### Strict Namespace Array

{% comment %}
```json
PUT /test/strict_namespace_array/doc

{
  "_strict": true,
  "tags": {
    "_namespace": true,
    "_type": "array/keyword",
    "field": {
      "subfield": {
        "_value": ["value1", "value2", "value3"],
      }
    }
  }
}
```
---
description: Index Strict Namespace Array
---

```json
GET /test/strict_namespace_array/._schema.schema.tags
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._type).to.equal('array/keyword');
});
```
---
description: Get Strict Namespace Array
---

```json
INFO /test/strict_namespace_array/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.tags.field.subfield).to.have.any.keys(['Kvalue1', 'Kvalue2', 'Kvalue3']);
  pm.expect(jsonData.terms.tags.subfield).to.have.any.keys(['Kvalue1', 'Kvalue2', 'Kvalue3']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1']);
});
```
---
description: Info Strict Namespace Array
---
{% endcomment %}


### Date type

{% comment %}
```json
PUT /test/date/doc

{
  "date": "2015/01/01 12:10:30"
}
```

---
description: Index date type format yyyy/mm/dd hh:mm:ss
---
```json
GET /test/date/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30');
});
```
---
description: Get date type format yyyy/mm/dd hh:mm:ss
---

```json
PUT /test/date/doc

{
  "date": "2015-01-01"
}
```

---
description: Index date type format yyyy-mm-dd
---
```json
GET /test/date/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.date).to.equal('2015-01-01T00:00:00');
});
```
---
description: Get date type format yyyy-mm-dd
---

```json
PUT /test/date/doc

{
  "date": "2015-01-01T12:10:30Z"
}
```

---
description: Index date type format yyyy-mm-ddThh:mm:ss
---
```json
GET /test/date/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30');
});
```
---
description: Get date type format yyyy-mm-ddThh:mm:ss
---

```json
PUT /test/date/doc

{
"date": 1420070400.001
}
```

---
description: Index date type format epoch
---
```json
GET /test/date/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.date).to.equal('2015-01-01T00:00:00.001');
});
```
---
description: Get date type format epoch
---
{% endcomment %}
