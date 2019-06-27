---
title: Range Search
---

### Test range search with field_terms

{% comment %}
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

---
description: Index document with type
---

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
description: Range search for field_terms
params: sort=foo
---
{% endcomment %}


### Test range search with field_values

{% comment %}
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

---
description: Index document with type
---

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
description: Range search for field_values
params: sort=foo
---
{% endcomment %}


### Test range search with field_all

{% comment %}
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

---
description: Index document with type
---

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
description: Range search for field_all
params: sort=foo
---
{% endcomment %}