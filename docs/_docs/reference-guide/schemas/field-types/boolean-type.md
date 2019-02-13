---
title: Boolean Datatype
short_title: Boolean
---

Boolean fields accept JSON true and false values, but can also accept strings
which are interpreted as either true or false but you have to explicitly use
the `_type` boolean:

{% capture req %}

```json
PUT /bank/1:search?pretty

{
    "is_published": true
}
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
PUT /bank/1:search?pretty

{
    "is_published": {
      "_value": "true",
      "_type": "boolean"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}

Also you can use in `_value` "true", "false", "t", "f", "1", "0"

The following parameters are accepted by text fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_value`                              | The value for the field                                                                 |
| `_slot`                               | The slot number                                                                         |
| `_index`                              | One or a pair of : `none`, `field_terms`, `field_values`, `field_all`, `field`, `global_terms`, `global_values`, `global_all`, `global`, `terms`, `values`, `all`      |
| `_prefix`                             | The prefix with the term is going to be indexed     |
| `_weight`                             | The weight with the term is going to be indexed     |