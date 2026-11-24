---
title: "Foreign Schemas"
---

Schema definition objects can be saved as either Metadata Schemas or Foreign
Schemas; by default, they are saved as Foreign Schemas, using foreign objects
that are stored inside the corresponding index in `.xapiand/indexes`.

To place the Schema object in some other index other than `.xapiand/indexes`,
you'd need to specify the object as foreign with an endpoint:

```rest
PUT /headlines/

{
  "_schema": {
    "_type": "foreign/object",
    "_foreign": ".schemas/319b4e5e-41af-4906-b3cd-cce91502dda3",
    "_id": {
      "_type": "keyword"
    },
    "title": {
      "_type": "text"
    }
  }
}
```

<!-- e2e:begin
---
description: Foreign Schemas: an index that writes the shared schema
---

```js
pm.test("The index answers its settings and schema", function() {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json()._schema).to.be.an("object");
});
```
e2e:end -->

In the example above, an index `.schemas` is created (if missing) containing
documents which can be used as shared Schemas. To exemplify differences about
IDs, note how the ID of the documents for such Schema objects is of type `"uuid"`
whilst the type of the ID for the documents in the `headlines` index itself is
`"keyword"`.

The Schema used for the index `headlines`, can be shared with other indexes that
use the same properties; for example, to create a new index `banners`, which
will have the exact same properties as `headlines`, use:

```rest
PUT /banners/

{
  "_schema": {
    "_type": "foreign/object",
    "_foreign": ".schemas/319b4e5e-41af-4906-b3cd-cce91502dda3"
  }
}
```

<!-- e2e:begin
---
description: Foreign Schemas: an index that uses it
---

```js
pm.test("The index answers its settings and schema", function() {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json()._schema).to.be.an("object");
});
```
e2e:end -->

A single shared Schema will be used by both indexes.
