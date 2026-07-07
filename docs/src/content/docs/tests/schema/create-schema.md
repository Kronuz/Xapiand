---
title: "Create Schema"
---

## Create Simple Schema

#### Create schema

```rest
PUT /test/create-schema/

{
  "_schema": {
    "name": {
      "_type": "text"
    },
    "age": {
      "_type": "positive"
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

#### Schema values are valid

```rest
GET /test/create-schema/
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
  pm.expect(jsonData._schema.name).to.include({'_type': 'text' });
  pm.expect(jsonData._schema.age).to.include({'_type': 'positive' });
});
```
e2e:end -->
