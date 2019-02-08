---
title: Cast Query
---

Cast allows explicit conversion from one data type to another as long as types
are compatible.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "balance" : {
      "_integer": 2221.82
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In the above example cast 2221.46 to integer, which later is internally
converted to 2221.0.

The following example will convert the integer 84535 to text:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "contact.postcode" : {
      "_text": 84535
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Type Compatibility

| Types                                 | Compatible Types                                                                        |
|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_integer`                            | `_positive`, `_float`, `_boolean`, `_text`                                              |
| `_positive`                           | `_integer`, `_float`, `_boolean`, `_text`                                               |
| `_float`                              | `_integer`, `_positive`, `_boolean`, `_text`                                            |
| `_boolean`                            | `_integer`, `_positive`, `_float`, `_text`                                              |
| `_text`                               | `_integer`, `_positive`, `_float`, `_boolean`, `_date`, `_time`, `_keyword` and objects |
| `_date` `_time` `_geospatial` `_uuid` | `_text`                                                                                 |
