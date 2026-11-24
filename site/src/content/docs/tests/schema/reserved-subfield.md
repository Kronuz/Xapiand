---
title: "Reserved Subfield"
---

## A reserved word inside a field

A field's specification takes only the reserved words of its type; any other
name that starts with `_` is refused with `400 Bad Request`
(`src/database/schema.cc`, "Field ... is not valid").

```rest
PUT /test/reserved_subfield/doc

{
  "name": {
    "_type": "text",
    "_reserved": "this is reserved, should fail"
  }
}
```

<!-- e2e:begin
---
description: Index Misuse Reserved Subfield
---

```js
pm.test("An unknown reserved word answers 400", function() {
  pm.response.to.have.status(400);
});
```
e2e:end -->
