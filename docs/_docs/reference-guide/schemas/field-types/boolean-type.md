---
title: Boolean Datatype
short_title: Boolean
---

Boolean fields accept JSON `true` and `false` values, but can also accept
strings which are interpreted as either `true` or `false`, but you have to
explicitly use the `_type` boolean. The two following examples are equivalent:


{% capture req %}

```json
PUT /bank/1?pretty

{
    "is_published": true
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
PUT /bank/1?pretty

{
    "is_published": {
      "_value": "true",
      "_type": "boolean"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}


When using strings, you can use any of the following and it gets interpreted
as either `true` or `false`:

* `"true"`, `"t"`, , `"1"` -> `true`
* `"false"`, `"f"`, `"0"` -> `false`


## Parameters

The following parameters are accepted by _Boolean_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |
