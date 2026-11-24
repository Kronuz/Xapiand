---
title: "Date type"
---

## Date type

#### Index date type format yyyy/mm/dd hh:mm:ss

```rest
PUT /test/date/doc

{
  "date": "2015/01/01 12:10:30"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get date type format yyyy/mm/dd hh:mm:ss

```rest
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30');
});
```
e2e:end -->

#### Index date type format yyyy-mm-dd

```rest
PUT /test/date/doc

{
  "date": "2015-01-01"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get date type format yyyy-mm-dd

<!-- e2e:begin
```rest
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
e2e:end -->

## Index date type format yyyy-mm-ddThh:mm:ss

```rest
PUT /test/date/doc

{
  "date": "2015-01-01T12:10:30Z"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get date type format yyyy-mm-ddThh:mm:ss

```rest
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30Z');
});
```
e2e:end -->

####  Index date type format epoch

```rest
PUT /test/date/doc

{
"date": 1420070400.001
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get date type format epoch

```rest
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T00:00:00.001');
});
```
e2e:end -->
