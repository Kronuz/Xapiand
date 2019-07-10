---
title: Foreign Schemas
---

Schema definition objects can be saved as either Metadata Schemas or Foreign
Schemas; by default, they are saved as Foreign Schemas, using foreign objects
that are stored inside the corresponding index in `.xapiand/indices`.

To place the Schema object in some other index other than `.xapiand/indices`,
you'd need to specify the object as foreign with an endpoint:

{% capture req %}

```json
PUT /headlines/

{
  "_schema": {
    "_type": "foreign/object",
    "_endpoint": ".schemas/319b4e5e-41af-4906-b3cd-cce91502dda3",
    "_id": {
      "_type": "uuid"
    },
    "schema": {
      "_id": {
        "_type": "string"
      },
      "title": {
        "_type": "text"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In the example above, an index `.schemas` is created (if missing) containing
documents which can be used as shared Schemas. To exemplify differences about
IDs, note how the ID of the documents for such Schema objects is of type `"uuid"`
whilst the type of the ID for the documents in the `headlines` index itself is
of type `"string"`.

The Schema used for the index `hedlines`, can be shared with other indexes that
use the same properties; for example, to create a new index `banners`, which
will have the exact same properties as `headlines`, use:

{% capture req %}

```json
PUT /banners/

{
  "_schema": {
    "_type": "foreign/object",
    "_endpoint": ".schemas/319b4e5e-41af-4906-b3cd-cce91502dda3"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A single shared Schema will be used by both indexes.
