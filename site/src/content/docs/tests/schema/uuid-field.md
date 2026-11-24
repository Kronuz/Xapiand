---
title: "Uuid Field"
---

## UUID Index Default

#### Index uuid field with default

```rest
PUT /test/uuid/uuid/doc

{
  "uuids": {
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
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

#### Get schema uuid field with default

```rest
GET /test/uuid/uuid/
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
  pm.expect(jsonData._schema.uuids).to.have.any.keys(['<uuid_field>']);
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_prefix']).to.equal('<uuid_field>.');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_type']).to.equal('text');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_slot']).to.equal(296138942);
});
```
e2e:end -->

#### Info uuid field with default

```rest
INFO /test/uuid/uuid/doc
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
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '296138942', '1407656467', '3856745852']);
});
```
e2e:end -->

#### Search uuid field with default

```rest
SEARCH /test/uuid/uuid/

{
  "_query": {
    "_and": [
      {
        "uuids.cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first"
      },
      {
        "uuids.50ffd30b-6b76-4827-b805-f37f5e95f093": "second"
      },
      {
        "uuids.b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
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
pm.test("Each uuid key is a field of its own", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search uuid field with default by the uuid field

```rest
SEARCH /test/uuid/uuid/

{
  "_query": {
    "uuids.<uuid_field>": "first"
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
pm.test("No shared uuid field is indexed", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## UUID Index uuid_field

#### Index uuid field with uuid_field

```rest
PUT /test/uuid/uuid_field/doc

{
  "uuids": {
    "_index_uuid_field": "uuid_field",
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
  }
}
```

#### Get schema uuid field with uuid_field

```rest
GET /test/uuid/uuid_field/
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
  pm.expect(jsonData._schema.uuids).to.have.any.keys(['<uuid_field>']);
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_prefix']).to.equal('<uuid_field>.');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_type']).to.equal('text');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_slot']).to.equal(2761136387);
});
```
e2e:end -->

#### Info uuid field with uuid_field

```rest
INFO /test/uuid/uuid_field/doc
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
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '2761136387']);
});
```
e2e:end -->

#### Search uuid field with uuid_field

```rest
SEARCH /test/uuid/uuid_field/

{
  "_query": {
    "_and": [
      {
        "uuids.<uuid_field>": "first"
      },
      {
        "uuids.<uuid_field>": "second"
      },
      {
        "uuids.<uuid_field>": "third"
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
pm.test("Every uuid key is indexed under the shared uuid field", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search uuid field with uuid_field by the uuid

```rest
SEARCH /test/uuid/uuid_field/

{
  "_query": {
    "uuids.cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first"
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
pm.test("No field of its own is indexed for each uuid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->

## UUID Index Field Both

#### Index uuid field with both

```rest
PUT /test/uuid/both/doc

{
  "uuids": {
    "_index_uuid_field": "both",
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third",
  }
}
```

#### Get schema uuid field with both

```rest
GET /test/uuid/both/
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
  pm.expect(jsonData._schema.uuids).to.have.any.keys(['<uuid_field>']);
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_prefix']).to.equal('<uuid_field>.');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_type']).to.equal('text');
  pm.expect(jsonData._schema.uuids['<uuid_field>']['_slot']).to.equal(2761136387);
});
```
e2e:end -->

#### Info uuid field with both

```rest
INFO /test/uuid/both/doc
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
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '296138942', '1407656467', '2761136387', '3856745852']);
});
```
e2e:end -->

#### Search uuid field with both

```rest
SEARCH /test/uuid/both/

{
  "_query": {
    "_and": [
      {
        "uuids.cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first"
      },
      {
        "uuids.50ffd30b-6b76-4827-b805-f37f5e95f093": "second"
      },
      {
        "uuids.b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
      },
      {
        "uuids.<uuid_field>": "first"
      },
      {
        "uuids.<uuid_field>": "second"
      },
      {
        "uuids.<uuid_field>": "third"
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
pm.test("Every uuid key is indexed both ways", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search uuid field with both for a word of another uuid

```rest
SEARCH /test/uuid/both/

{
  "_query": {
    "uuids.50ffd30b-6b76-4827-b805-f37f5e95f093": "first"
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
pm.test("A uuid key finds only its own word", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->
