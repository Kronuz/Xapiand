---
title: "Ignore Comments"
---

## Ignore Comments

#### Ignore comment

```rest
POST /test/comment-ignore/

{
  "_recurse": false,
  "_id": {
    "_type": "uuid",
  },
  "#comment": "This comment is ignored",
  "_schema": {
    "_type": "object"
  }
}
```

A key that starts with `#` is a comment: the schema skips it, so it is neither a
field of the document nor of the schema.

<!-- e2e:begin
---
description: Index a document with a comment
---

```js
pm.test("The document with a comment is indexed", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### The schema has no comment field

```rest
GET /test/comment-ignore/
```

<!-- e2e:begin
---
description: The comment is not in the schema
---

```js
pm.test("The schema has no comment field", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema).to.be.an("object");
  var seen = [];
  (function walk(obj) {
    for (var key in obj) {
      seen.push(key);
      if (obj[key] && typeof obj[key] === "object") { walk(obj[key]); }
    }
  })(jsonData._schema);
  pm.expect(seen.length).to.be.above(0);
  seen.forEach(function(key) { pm.expect(key).to.not.include("#comment"); });
});
```
e2e:end -->
