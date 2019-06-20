---
title: Namespace
---

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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '271287252', '439783812', '1032232283', '2277530749', '2821621140']);
});
```
---
description: Info Namespace
---
{% endcomment %}


### Namespace No Partial Paths

{% comment %}
```json
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
---
description: Index Namespace No Partial Paths
---

```json
GET /test/namespace/no_partial_paths/._schema.schema.style
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
  pm.expect(jsonData._partial_paths).to.equal(false);
});
```
---
description: Get Namespace No Partial Paths
---

```json
INFO /test/namespace/no_partial_paths/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.style.clothing.footwear).to.have.all.keys(['Scasual', 'Sshoes']);
  pm.expect(jsonData.terms.style.clothing.pants).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms.style.hairstyle).to.have.all.keys(['Safro']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '333121230', '439783812', '2277530749']);
});
```

---
description: Info Namespace No Partial Paths
---
{% endcomment %}


### Namespace with Key _index

{% comment %}
```json
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
---
description: Index Namespace With Key _index
---

```json
GET /test/namespace/index_namespace/._schema.schema.style
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
description: Get Namespace With Key _index
---

```json
INFO /test/namespace/index_namespace/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '271287252', '439783812', '1032232283', '2277530749', '2821621140']);
});
```
---
description: Info Namespace With Key _index
---
{% endcomment %}


### Strict Namespace

{% comment %}
```json
PUT /test/namespace/strict_namespace/doc

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
GET /test/namespace/strict_namespace/._schema.schema.tags
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
INFO /test/namespace/strict_namespace/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3362632514', '3554249428']);
});
```
---
description: Info Strict Namespace
---
{% endcomment %}


### Strict Namespace Array

{% comment %}
```json
PUT /test/namespace/strict_namespace_array/doc

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
GET /test/namespace/strict_namespace_array/._schema.schema.tags
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
INFO /test/namespace/strict_namespace_array/doc
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
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3362632514', '3554249428']);
});
```
---
description: Info Strict Namespace Array
---
{% endcomment %}


### Namespace different nested types

{% comment %}
```json
PUT /test/namespace/namespace_different/doc

{
  "_strict": true,
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
---
description: Index Namespace Different Nested Types
---
```js
pm.test("Response is expected error", function() {
  pm.expect(pm.response.code).to.equal(400);
});
```
{% endcomment %}


### Namespace text type

{% comment %}
```json
PUT /test/namespace/namespace_text/doc

{
  "_strict": true,
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
---
description: Index Namespace Text Type
---

```json
GET /test/namespace/namespace_text/._schema.schema.tags
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
  pm.expect(jsonData._type).to.equal('text');
});
```

---
description: Get Namespace Text Type
---

```json
INFO /test/namespace/namespace_text/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.tags.field.subfield).that.have.all.keys(['Sa', 'Sis', 'Sthis', 'Svalue', 'term_freq', 'wdf']);
  pm.expect(jsonData.terms.tags.subfield).to.have.any.keys(['Sa', 'Sis', 'Sthis', 'Svalue', 'term_freq', 'wdf']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1380202056', '1586983639']);
});
```
---
description: Info Strict Namespace Array
---
{% endcomment %}


### Namespace datetime type
{% comment %}
```json
PUT /test/namespace/namespace_datetime/doc

{
  "_strict": true,
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
---
description: Index Namespace Datetime Type
---

```json
GET /test/namespace/namespace_datetime/._schema.schema.tags
```

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

---
description: Get Namespace Datetime Type
---

```json
INFO /test/namespace/namespace_datetime/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  var field_name = '';
  field_name = String.fromCharCode(68) + String.fromCharCode(368) + String.fromCharCode(1671);
  pm.expect(jsonData.terms.tags.field.subfield['<12cc0300>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(24) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.field.subfield['<15180>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(372) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.field.subfield['<1e13380>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(373) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.field.subfield['<278d00>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(368) + String.fromCharCode(1671);
  pm.expect(jsonData.terms.tags.field.subfield['<bbf81e00>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(25) + String.fromCharCode(65533) + String.fromCharCode(64);
  pm.expect(jsonData.terms.tags.field.subfield['<e10>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(25) + String.fromCharCode(682) +String.fromCharCode(62) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.field.subfield).to.have.any.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(368) + String.fromCharCode(1671);
  pm.expect(jsonData.terms.tags.subfield['<12cc0300>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(24) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.subfield['<15180>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(372) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.subfield['<1e13380>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(373) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.subfield['<278d00>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(368) + String.fromCharCode(1671);
  pm.expect(jsonData.terms.tags.subfield['<bbf81e00>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(25) + String.fromCharCode(65533) + String.fromCharCode(64);
  pm.expect(jsonData.terms.tags.subfield['<e10>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(68) + String.fromCharCode(374) + String.fromCharCode(25) + String.fromCharCode(682) +String.fromCharCode(62) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.tags.subfield).to.have.any.keys([field_name]);
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '760467980', '2973955702']);
  });
```
---
description: Info Namespace Datetime Type
---
{% endcomment %}


### Namespace Numeric type

{% comment %}
```json
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

---
description: Index Namespace Numeric Type
---

```json
GET /test/namespace/types/integer/._schema.schema.style
```

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

---
description: Get Namespace Numeric Type
---

```json
INFO /test/namespace/types/integer/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  tobytes = (a) => {
    let buff = [];
    for (let i = 0; i < a.length; i++) {
        buff.push(a[i].charCodeAt(0));
    }
    return buff;
  }

  var field_name = '';
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.footwear['<186a0>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.footwear['<2710>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.footwear['<3e8>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.footwear['<5f5e100>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) +  String.fromCharCode(100);
  pm.expect(jsonData.terms.style.clothing.footwear['<64>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.footwear['<f4240>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(114);
  pm.expect(jsonData.terms.style.clothing.footwear).to.have.property(field_name);

  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<186a0>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<2710>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<3e8>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<5f5e100>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) +  String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<64>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants['<f4240>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.clothing.pants).to.have.property(field_name);

  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.footwear['<186a0>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.footwear['<2710>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.footwear['<3e8>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.footwear['<5f5e100>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) +  String.fromCharCode(100);
  pm.expect(jsonData.terms.style.footwear['<64>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.footwear['<f4240>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(114);
  pm.expect(jsonData.terms.style.footwear).to.have.property(field_name);

  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<186a0>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<2710>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<3e8>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<5f5e100>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) +  String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<64>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants['<f4240>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.pants).to.have.property(field_name);

  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<186a0>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<2710>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<3e8>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<5f5e100>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) +  String.fromCharCode(65533) +  String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<64>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle['<f4240>']).that.have.all.keys([field_name]);
  field_name = String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms.style.hairstyle).to.have.property(field_name);

  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '328441664', '1844154444', '2306662042', '2533366391', '3009507063']);
});
```
---
description: Info Namespace Numeric Type
---

```json
SEARCH /test/namespace/types/integer/

{
  "_query": {
    "style.clothing.footwear": 456
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
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.eql('doc');
  pm.expect(jsonData.hits[0]).to.have.property('style');
});
```

---
description: Search Namespace Numeric Type
---


```json
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

---
description: Search Namespace Numeric type by Range
---
{% endcomment %}


### Namespace Geospatial type

{% comment %}
```json
PUT /test/namespace/types/geospatial/doc

{
  "_strict": true,
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

---
description: Index Namespace Geospatial Type
---

```json
GET /test/namespace/types/geospatial/._schema.schema.tags
```

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

---
description: Get Namespace Geospatial Type
---

```json
INFO /test/namespace/types/geospatial/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.tags.field).that.have.all.keys(['<3>', '<5>', '<8>', '<a>', '<c>', '<f>', 'G\u0000', 'term_freq', 'wdf']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '3171062315']);
});
```
---
description: Info Namespace Geospatial Type
---
{% endcomment %}