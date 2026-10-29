---
title: "Object"
---

## Object of Mixed Types

#### Index Mixed Objects

```rest
PUT /test/mixed_objects/doc

{
  "types": {
    "type": "human",
    "legs": 2,
    "arms": 2,
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Mixed Objects

```rest
GET /test/mixed_objects/
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
  pm.expect(jsonData._schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.types.arms._type).to.equal('integer');
});
```
e2e:end -->

#### Info Mixed Objects

```rest
INFO /test/mixed_objects/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1663382011','3248593248', '3741895486']);
});
```
e2e:end -->

#### Search Mixed Objects

```rest
SEARCH /test/mixed_objects/

{
  "_query": {
    "_and": [
      {
        "types.type": "human"
      },
      {
        "types.legs": 2
      },
      {
        "types.arms": 2
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
pm.test("The text and numbers of the object are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Mixed Objects by range

```rest
SEARCH /test/mixed_objects/

{
  "_query": {
    "_and": [
      {
        "types.legs": {
          "_in": {
            "_range": {
              "_from": 1,
              "_to": 3
            }
          }
        }
      },
      {
        "types.arms": {
          "_in": {
            "_range": {
              "_from": 1,
              "_to": 3
            }
          }
        }
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
pm.test("The numbers of the object are found by range", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Mixed Objects out of range

```rest
SEARCH /test/mixed_objects/

{
  "_query": {
    "types.arms": {
      "_in": {
        "_range": {
          "_from": 5,
          "_to": 9
        }
      }
    }
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
pm.test("A range that does not hold the number finds nothing", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## Value With Nested Object

#### Index value with nested object

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get value with nested object

```rest
GET /test/value_object_nested/
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
  pm.expect(jsonData._schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.types.arms._type).to.equal('integer');
  pm.expect(jsonData._schema.types.name._type).to.equal('object');
  pm.expect(jsonData._schema.types.name.first._type).to.equal('text');
  pm.expect(jsonData._schema.types.name.last._type).to.equal('text');
});
```
e2e:end -->

## Simple Object

#### Index Object

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Object

```rest
GET /test/object/._schema.name
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
  pm.expect(jsonData._type).to.equal('object');
});
```
e2e:end -->

#### Search Object

```rest
SEARCH /test/object/

{
  "_query": {
    "_and": [
      {
        "name.first": "john"
      },
      {
        "name.middle": "r"
      },
      {
        "name.last": "doe"
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
pm.test("The words of the object are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Object for another name

```rest
SEARCH /test/object/

{
  "_query": {
    "name.first": "jane"
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
pm.test("A word the object does not hold finds nothing", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## Nested Object

#### Index Nested Object

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Nested Object

```rest
GET /test/nested_object/._schema.name
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
  pm.expect(jsonData._type).to.equal('object');
});
```
e2e:end -->

#### Search Nested Object

```rest
SEARCH /test/nested_object/

{
  "_query": {
    "_and": [
      {
        "name.first": "john"
      },
      {
        "name.middle": "r"
      },
      {
        "name.last": "doe"
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
pm.test("The words of the nested object are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Complex Object

#### Index Complex Object

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Complex Object

```rest
GET /test/complex_object/._schema
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
e2e:end -->

#### Info Complex Object

```rest
INFO /test/complex_object/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Search Complex Object

```rest
SEARCH /test/complex_object/

{
  "_query": {
    "_and": [
      {
        "personality": "behavior"
      },
      {
        "personality": "cherish"
      },
      {
        "personality": "elegant"
      },
      {
        "personality": "flaw"
      },
      {
        "personality": "disrupt"
      },
      {
        "personality": "insulting"
      },
      {
        "contact.address": "630"
      },
      {
        "contact.address": "victor"
      },
      {
        "contact.address": "road"
      },
      {
        "contact.city": "leyner"
      },
      {
        "contact.email": "michael"
      },
      {
        "contact.email": "globoil"
      },
      {
        "contact.email": "uk"
      },
      {
        "contact.phone": "594"
      },
      {
        "contact.phone": "3216"
      },
      {
        "contact.postcode": "61952"
      },
      {
        "contact.state": "indiana"
      },
      {
        "employer": "globoil"
      },
      {
        "eyeColor": "blue"
      },
      {
        "favoriteFruit": "lemon"
      },
      {
        "gender": "male"
      },
      {
        "name.firstName": "michael"
      },
      {
        "name.lastName": "lee"
      },
      {
        "style.clothing.pants": "khakis"
      },
      {
        "style.clothing.shirt": "shirt"
      },
      {
        "style.hairstyle": "slick"
      },
      {
        "style.hairstyle": "back"
      },
      {
        "style.pants": "khakis"
      },
      {
        "style.shirt": "shirt"
      },
      {
        "accountNumber": 121931
      },
      {
        "age": 24
      },
      {
        "balance": 221.46
      },
      {
        "accountNumber": {
          "_in": {
            "_range": {
              "_from": 100000,
              "_to": 130000
            }
          }
        }
      },
      {
        "age": {
          "_in": {
            "_range": {
              "_from": 20,
              "_to": 30
            }
          }
        }
      },
      {
        "balance": {
          "_in": {
            "_range": {
              "_from": 200,
              "_to": 250
            }
          }
        }
      },
      {
        "checkin": {
          "_point": {
            "_longitude": -95.63079,
            "_latitude": 31.76212
          }
        }
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
pm.test("Every field of the object is searchable, the personality by the stems of its words", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Complex Object for words that are not indexed

```rest
SEARCH /test/complex_object/

{
  "_query": {
    "_and": [
      {
        "personality": "behavior"
      },
      {
        "personality": "the"
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
pm.test("A stop word of the personality is not indexed", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

#### Search Complex Object out of range

```rest
SEARCH /test/complex_object/

{
  "_query": {
    "age": {
      "_in": {
        "_range": {
          "_from": 30,
          "_to": 40
        }
      }
    }
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
pm.test("A range that does not hold the age finds nothing", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->
