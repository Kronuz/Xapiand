---
title: Date type
---

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
