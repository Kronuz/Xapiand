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
