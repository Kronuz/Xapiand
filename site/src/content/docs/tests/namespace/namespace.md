---
title: "Namespace"
---

## Namespace

#### Index Namespace

```rest
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Namespace

```rest
GET /test/namespace/._schema.style
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
  pm.expect(jsonData._namespace).to.equal(true);
});
```
e2e:end -->

#### Info Namespace

```rest
INFO /test/namespace/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '271287252', '439783812', '1032232283', '2277530749', '2821621140']);
});
```
e2e:end -->

#### Search Namespace

```rest
SEARCH /test/namespace/

{
  "_query": {
    "_and": [
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
pm.test("Each path and its partial paths are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Namespace No Partial Paths

#### Index Namespace No Partial Paths

```rest
PUT /test/namespace/no_partial_paths/doc

{
  "style": {
    "_namespace": true,
    "_partial_paths": false,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
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

#### Get Namespace No Partial Paths

```rest
GET /test/namespace/no_partial_paths/._schema.style
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
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._partial_paths).to.equal(false);
});
```
e2e:end -->

#### Info Namespace No Partial Paths

```rest
INFO /test/namespace/no_partial_paths/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '333121230', '439783812', '2277530749']);
});
```
e2e:end -->

#### Search Namespace No Partial Paths

```rest
SEARCH /test/namespace/no_partial_paths/

{
  "_query": {
    "_and": [
      {
        "style.clothing.footwear": "casual"
      },
      {
        "style.clothing.footwear": "shoes"
      },
      {
        "style.clothing.pants": "khakis"
      },
      {
        "style.hairstyle": "afro"
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
pm.test("Each full path is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Namespace No Partial Paths by a partial path

```rest
SEARCH /test/namespace/no_partial_paths/

{
  "_query": {
    "style.pants": "khakis"
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
pm.test("A partial path is not indexed", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## Namespace with Key _index

#### Index Namespace With Key _index

```rest
PUT /test/namespace/index_namespace/doc

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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Namespace With Key _index

```rest
GET /test/namespace/index_namespace/._schema.style
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
  pm.expect(jsonData._namespace).to.equal(true);
});
```
e2e:end -->

#### Info Namespace With Key _index

```rest
INFO /test/namespace/index_namespace/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '271287252', '439783812', '1032232283', '2277530749', '2821621140']);
});
```
e2e:end -->

#### Search Namespace With Key _index

```rest
SEARCH /test/namespace/index_namespace/

{
  "_query": {
    "_and": [
      {
        "style.clothing.pants": "khakis"
      },
      {
        "style.pants": "khakis"
      },
      {
        "style.hairstyle": "slick"
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
pm.test("Each path and its partial paths are searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Strict Namespace

#### Index Strict Namespace

```rest
PUT /test/namespace/strict_namespace/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Strict Namespace

```rest
GET /test/namespace/strict_namespace/._schema.tags
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
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._type).to.equal('keyword');
});
```
e2e:end -->

#### Info Strict Namespace

```rest
INFO /test/namespace/strict_namespace/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3362632514', '3554249428']);
});
```
e2e:end -->

#### Search Strict Namespace

```rest
SEARCH /test/namespace/strict_namespace/

{
  "_query": {
    "_and": [
      {
        "tags.field.subfield": "value"
      },
      {
        "tags.subfield": "value"
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
pm.test("The keyword is searchable under the path and its partial path", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Strict Namespace Array

#### Index Strict Namespace Array

```rest
PUT /test/namespace/strict_namespace_array/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
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

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get Strict Namespace Array

```rest
GET /test/namespace/strict_namespace_array/._schema.tags
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
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._type).to.equal('array/keyword');
});
```
e2e:end -->

#### Info Strict Namespace Array

```rest
INFO /test/namespace/strict_namespace_array/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3362632514', '3554249428']);
});
```
e2e:end -->

#### Search Strict Namespace Array

```rest
SEARCH /test/namespace/strict_namespace_array/

{
  "_query": {
    "_and": [
      {
        "tags.field.subfield": "value1"
      },
      {
        "tags.subfield": "value1"
      },
      {
        "tags.field.subfield": "value2"
      },
      {
        "tags.subfield": "value2"
      },
      {
        "tags.field.subfield": "value3"
      },
      {
        "tags.subfield": "value3"
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
pm.test("Every keyword of the array is searchable under the path and its partial path", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Namespace different nested types

#### Index Namespace Different Nested Types

```rest
PUT /test/namespace/namespace_different/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
  "tags": {
    "_namespace": true,
    "_type": "array/keyword",
    "field": {
      "subfield": {
        "_value": ["value1", "value2", "value3"],
      }
    },
    "numeric_field": 10
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is expected error", function() {
  pm.expect(pm.response.code).to.equal(400);
});
```
e2e:end -->

## Namespace text type

#### Index Namespace Text Type

```rest
PUT /test/namespace/namespace_text/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
  "tags": {
    "_namespace": true,
    "_type": "text",
    "field": {
      "subfield": {
        "_value": "This is a value",
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

#### Get Namespace Text Type

```rest
GET /test/namespace/namespace_text/._schema.tags
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
  pm.expect(jsonData._namespace).to.equal(true);
  pm.expect(jsonData._type).to.equal('text');
});
```
e2e:end -->

#### Info Namespace Text

```rest
INFO /test/namespace/namespace_text/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1380202056', '1586983639']);
});
```
e2e:end -->

#### Search Namespace Text Type

```rest
SEARCH /test/namespace/namespace_text/

{
  "_query": {
    "_and": [
      {
        "tags.field.subfield": "this"
      },
      {
        "tags.field.subfield": "is"
      },
      {
        "tags.field.subfield": "a"
      },
      {
        "tags.field.subfield": "value"
      },
      {
        "tags.subfield": "value"
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
pm.test("Every word of the text is searchable under the path and its partial path", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

## Namespace datetime type

#### Index Namespace Datetime Type

```rest
PUT /test/namespace/namespace_datetime/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
  "tags": {
    "_namespace": true,
    "_type": "datetime",
    "field": {
      "subfield": {
        "_value": "2001-05-24T10:41:25.123Z"
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

#### Get Namespace Datetime Type

```rest
GET /test/namespace/namespace_datetime/._schema.tags
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
  pm.expect(jsonData._type).to.equal('datetime');
  pm.expect(jsonData._accuracy_prefix).to.eql(['<e10>.','<15180>.','<278d00>.','<1e13380>.','<12cc0300>.','<bbf81e00>.']);
  pm.expect(jsonData._accuracy).to.eql(['hour', 'day', 'month', 'year', 'decade', 'century']);
});
```
e2e:end -->

#### Info Namespace Datetime Type

```rest
INFO /test/namespace/namespace_datetime/doc
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
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '760467980', '2973955702']);
  });
```
e2e:end -->

#### Search Namespace Datetime Type

```rest
SEARCH /test/namespace/namespace_datetime/

{
  "_query": {
    "_and": [
      {
        "tags.field.subfield": "2001-05-24T10:41:25.123Z"
      },
      {
        "tags.subfield": "2001-05-24T10:41:25.123Z"
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
pm.test("The datetime is searchable under the path and its partial path", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Namespace Datetime Type by range

```rest
SEARCH /test/namespace/namespace_datetime/

{
  "_query": {
    "_and": [
      {
        "tags.field.subfield": {
          "_in": {
            "_range": {
              "_from": "2001-05-01",
              "_to": "2001-05-31"
            }
          }
        }
      },
      {
        "tags.subfield": {
          "_in": {
            "_range": {
              "_from": "2001-01-01",
              "_to": "2001-12-31"
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
pm.test("The datetime is found by ranges around it", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Namespace Datetime Type out of range

```rest
SEARCH /test/namespace/namespace_datetime/

{
  "_query": {
    "tags.field.subfield": {
      "_in": {
        "_range": {
          "_from": "2002-01-01",
          "_to": "2002-12-31"
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
pm.test("A range that does not hold the datetime finds nothing", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## Namespace Numeric type

#### Index Namespace Numeric Type

```rest
PUT /test/namespace/types/integer/doc

{
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": 123,
      "footwear": 456
    },
    "hairstyle": 789
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

#### Get Namespace Numeric Type

```rest
GET /test/namespace/types/integer/._schema.style
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
  pm.expect(jsonData._type).to.equal('integer');
  pm.expect(jsonData._accuracy_prefix).to.eql(['<64>.','<3e8>.','<2710>.','<186a0>.','<f4240>.','<5f5e100>.']);
  pm.expect(jsonData._accuracy).to.eql([100, 1000, 10000, 100000, 1000000, 100000000]);
});
```
e2e:end -->

####  Info Namespace Numeric Type

```rest
INFO /test/namespace/types/integer/doc
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
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '328441664', '1844154444', '2306662042', '2533366391', '3009507063']);
});
```
e2e:end -->

#### Search Namespace Numeric Type by every path

```rest
SEARCH /test/namespace/types/integer/

{
  "_query": {
    "_and": [
      {
        "style.clothing.pants": 123
      },
      {
        "style.clothing.footwear": 456
      },
      {
        "style.hairstyle": 789
      },
      {
        "style.pants": 123
      },
      {
        "style.footwear": 456
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
pm.test("Every path and partial path finds its number", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Namespace Numeric Type by partial path ranges

```rest
SEARCH /test/namespace/types/integer/

{
  "_query": {
    "_and": [
      {
        "style.pants": {
          "_in": {
            "_range": {
              "_from": 100,
              "_to": 200
            }
          }
        }
      },
      {
        "style.footwear": {
          "_in": {
            "_range": {
              "_from": 450,
              "_to": 460
            }
          }
        }
      },
      {
        "style.hairstyle": {
          "_in": {
            "_range": {
              "_from": 700,
              "_to": 800
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
pm.test("Every partial path finds its number by range", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search Namespace Numeric Type

```rest
SEARCH /test/namespace/types/integer/

{
  "_query": {
    "style.clothing.footwear": 456
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.eql('doc');
  pm.expect(jsonData.hits[0]).to.have.property('style');
});
```
e2e:end -->

#### Search Namespace Numeric type by Range

```rest
SEARCH /test/namespace/types/integer/

{
  "_query": {
    "style.clothing.footwear": {
      "_in": {
        "_range": {
          "_from": 450,
          "_to": 460
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.eql('doc');
  pm.expect(jsonData.hits[0]).to.have.property('style');
});
```
e2e:end -->

## Namespace Geospatial type

#### Index Namespace Geospatial Type

```rest
PUT /test/namespace/types/geospatial/doc

{
  "_strict": true,
  "_settings": {
    "number_of_shards": 5,
    "number_of_replicas": 1
  },
  "tags": {
    "_namespace": true,
    "_type": "geospatial",
    "field": {
      "_point": {
        "_longitude": -80.31727,
        "_latitude": 25.67927
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

#### Get Namespace Geospatial Type

```rest
GET /test/namespace/types/geospatial/._schema.tags
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
  pm.expect(jsonData._type).to.equal('geo');
  pm.expect(jsonData._accuracy_prefix).to.eql(['<3>.','<5>.','<8>.','<a>.','<c>.','<f>.']);
  pm.expect(jsonData._accuracy).to.eql([3, 5, 8, 10, 12, 15]);
  pm.expect(jsonData._namespace).to.equal(true);
});
```
e2e:end -->

#### Info Namespace Geospatial Type

```rest
INFO /test/namespace/types/geospatial/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3171062315']);
});
```
e2e:end -->

#### Search Namespace Geospatial Type

```rest
SEARCH /test/namespace/types/geospatial/

{
  "_query": {
    "tags.field": {
      "_point": {
        "_longitude": -80.31727,
        "_latitude": 25.67927
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
pm.test("The point is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->
