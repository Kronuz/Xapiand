---
title: UUID Datatype
short_title: UUID
---

A UUID field can store version 1, 3, 4, and 5 UUIDs as specified in [RFC 4122]{:target="_blank"}.

{% capture req %}

```json
UPDATE /bank/1

{
  "guid": "1f1e8f3f-471f-4b3d-a2a4-7aefb43a9087"
}
```
{% endcapture %}
{% include curl.html req=req %}


## Condensed UUIDs

Xapiand supports a subset of UUID 1 which can be represented as strings in
condensed form. Document IDs are of this type by default.

For example, the UUID `00000000-0000-1000-8000-010000000000` can be represented
as the string "~notmet".


## UUIDs as Field Names

{% capture req %}

```json
UPDATE /bank/1

{
  "uuids": {
    "a8098c1a-f86e-11da-bd1a-00112444be1e": "uuid1",
    "6fa459ea-ee8a-3ca4-894e-db77e160355e": "uuid3",
    "16fd2706-8baf-433b-82eb-8c7fada847da": "uuid4",
    "886313e1-3b8a-5372-9b90-0c9aee199e5d": "uuid5"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

`_index_uuid_field` can be used to specify how the UUID fields will be indexed:

+ The default, type "uuid", stores the field name as the UUID value itself.
+ The type "uuid_field" stores the field name as the string "<uuid_field>".
+ The type "both" stores the field name as both the UUID value and as "<uuid_field>".


[RFC 4122]: https://tools.ietf.org/html/rfc4122.html
