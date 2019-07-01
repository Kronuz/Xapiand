---
title: Check Datatypes
---

## Check All Types


#### Index Datatypes


{% capture req %}

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
{% endcapture %}
{% include curl.html req=req %}


{% comment %}
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
{% endcomment %}


#### Check Datatypes

{% capture req %}

```json
GET /test/types/
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

#### Info Datatypes

{% capture req %}

```json
INFO /test/types/~notmet
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
  pm.expect(jsonData.terms).to.have.property('QU\u001c\u0000\u0000\u0001');
  pm.expect(jsonData.terms.boolean).that.have.all.keys(['Bt']);
  pm.expect(jsonData.terms.keyword).that.have.all.keys(['Ktrue']);
  pm.expect(jsonData.terms.uuid).that.have.all.keys(['U\u0007\u020dZ\u001dw\ufffdW']);
  pm.expect(jsonData.terms.text).that.have.all.keys(['Sfield', 'Sis', 'Stext', 'Sthis']);

  pm.expect(jsonData.terms.date).that.have.all.keys(['<12cc0300>', '<15180>', '<1e13380>', '<278d00>', '<bbf81e00>', '<e10>', 'D\ufffd\ufffd\ufffd\ufffd\ufffd']);
  pm.expect(jsonData.terms.date['<12cc0300>']).that.have.all.keys(['D\ufffd\ufffd=;']);
  pm.expect(jsonData.terms.date['<15180>']).that.have.all.keys(['D\ufffd\ufffd\ufffd\ufffd\ufffd']);
  pm.expect(jsonData.terms.date['<1e13380>']).that.have.all.keys(['D\ufffd\ufffd*\ufffd\ufffd']);
  pm.expect(jsonData.terms.date['<278d00>']).that.have.all.keys(['D\ufffd\ufffd\ufffd\ufffd\ufffd']);
  pm.expect(jsonData.terms.date['<bbf81e00>']).that.have.all.keys(['D\u0170\u0687']);
  pm.expect(jsonData.terms.date['<e10>']).that.have.any.keys(['D\ufffd\ufffd\ufffd\ufffd\ufffd']);

  pm.expect(jsonData.terms.datetime).that.have.all.keys(['<12cc0300>', '<15180>', '<1e13380>', '<278d00>', '<bbf81e00>', '<e10>', 'D\ufffd\ufffd\u0788\ufffd\u001f\u007c\ufffd']);
  pm.expect(jsonData.terms.datetime['<12cc0300>']).that.have.all.keys(['D\ufffd\ufffd=;']);
  pm.expect(jsonData.terms.datetime['<15180>']).that.have.all.keys(['D\ufffd\ufffd\ufffd\ufffd\ufffd']);
  pm.expect(jsonData.terms.datetime['<1e13380>']).that.have.all.keys(['D\ufffd\ufffd*\ufffd\ufffd']);
  pm.expect(jsonData.terms.datetime['<278d00>']).that.have.all.keys(['D\ufffd\ufffd\ufffd\ufffd\ufffd']);
  pm.expect(jsonData.terms.datetime['<bbf81e00>']).that.have.all.keys(['D\u0170\u0687']);
  pm.expect(jsonData.terms.datetime['<e10>']).that.have.all.keys(['D\ufffd\ufffd\u0786\u0020']);

  pm.expect(jsonData.terms.floating).that.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', 'N\ufffd\u0078\ufffd']);
  pm.expect(jsonData.terms.floating['<186a0>']).that.have.all.keys(['N\ufffda\ufffd']);
  pm.expect(jsonData.terms.floating['<2710>']).that.have.all.keys(['N\ufffdu0']);
  pm.expect(jsonData.terms.floating['<3e8>']).that.have.all.keys(['N\ufffdx\u001e']);
  pm.expect(jsonData.terms.floating['<5f5e100>']).that.have.all.keys(['N\ufffd']);
  pm.expect(jsonData.terms.floating['<64>']).that.have.all.keys(['N\ufffdx\ufffd']);
  pm.expect(jsonData.terms.floating['<f4240>']).that.have.all.keys(['N\ufffd']);

  pm.expect(jsonData.terms.integer).that.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', 'N\ufffd\u0078\ufffd']);
  pm.expect(jsonData.terms.integer['<186a0>']).that.have.all.keys(['N\ufffda\ufffd']);
  pm.expect(jsonData.terms.integer['<2710>']).that.have.all.keys(['N\ufffdu0']);
  pm.expect(jsonData.terms.integer['<3e8>']).that.have.all.keys(['N\ufffdx\u001e']);
  pm.expect(jsonData.terms.integer['<5f5e100>']).that.have.all.keys(['N\ufffd']);
  pm.expect(jsonData.terms.integer['<64>']).that.have.all.keys(['N\ufffdx\ufffd']);
  pm.expect(jsonData.terms.integer['<f4240>']).that.have.all.keys(['N\ufffd']);

  pm.expect(jsonData.terms.positive).that.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', 'N\ufffd\u0078\ufffd']);
  pm.expect(jsonData.terms.positive['<186a0>']).that.have.all.keys(['N\ufffda\ufffd']);
  pm.expect(jsonData.terms.positive['<2710>']).that.have.all.keys(['N\ufffdu0']);
  pm.expect(jsonData.terms.positive['<3e8>']).that.have.all.keys(['N\ufffdx\u001e']);
  pm.expect(jsonData.terms.positive['<5f5e100>']).that.have.all.keys(['N\ufffd']);
  pm.expect(jsonData.terms.positive['<64>']).that.have.all.keys(['N\ufffdx\ufffd']);
  pm.expect(jsonData.terms.positive['<f4240>']).that.have.all.keys(['N\ufffd']);

  pm.expect(jsonData.terms.time).that.have.all.keys(['<3c>', '<e10>', 'T\ufffd\u0007\ufffd\u000f\ufffdv\u0234']);
  pm.expect(jsonData.terms.time['<3c>']).that.have.all.keys(['N\ufffd\u0007\ufffd']);
  pm.expect(jsonData.terms.time['<e10>']).that.have.all.keys(['N\ufffd\u0006P']);

  pm.expect(jsonData.terms.timedelta).that.have.all.keys(['<3c>', '<e10>', 'T\ufffd\u0007\ufffd\u000f\ufffdv\u0234']);
  pm.expect(jsonData.terms.timedelta['<3c>']).that.have.all.keys(['N\ufffd\u0007\ufffd']);
  pm.expect(jsonData.terms.timedelta['<e10>']).that.have.all.keys(['N\ufffd\u0006P']);

  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '318005193', '774410579', '1446142349', '1627686662', '1998353627', '2090108769', '2153937510', '2668794980', '3480268142', '3547094521', '3917830578']);
});
```
{% endcomment %}
