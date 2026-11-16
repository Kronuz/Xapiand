---
title: "Open / Close Index APIs"
---

The `CLOSE` and `OPEN` commands are routed but not implemented: both answer
`501 Not Implemented` and leave the index as it is (`src/server/search_views.cc`,
the `OPEN` and `CLOSE` cases of the method switch).

```rest
CLOSE /my_index/
```

<!-- e2e:begin
---
description: Close: not implemented
---

```js
pm.test("CLOSE answers 501 Not Implemented", function() {
  pm.response.to.have.status(501);
});
```
e2e:end -->

```rest
OPEN /my_index/
```

<!-- e2e:begin
---
description: Open: not implemented
---

```js
pm.test("OPEN answers 501 Not Implemented", function() {
  pm.response.to.have.status(501);
});
```
e2e:end -->
