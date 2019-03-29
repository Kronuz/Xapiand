---
title: Dynamic Typing
---

{: .note .construction }
_This section is a **work in progress**..._

One of the most important features of Xapiand is that it tries to get out of
your way and let you start exploring your data as quickly as possible. To index
a document, you don't have to first create an index, define a schema, and
define your fields; you can just index a document and the index, schema, and
fields will spring to life automatically:

{% capture req %}

```json
PUT /my_index/1

{
  "name": "John Doe",
  "age": 20
}
```
{% endcapture %}
{% include curl.html req=req %}

Creates the `/my_index/` index, a schema with two fields: a field called `name`
with datatype `text` and a field called `count` with datatype `integer`.

This automatic detection and addition of new fields is called dynamic typing.

# Datatype Detection

By default, when a previously unseen field is found in a document, Xapiand will
add the new field to the schema. This behaviour can be disabled globally by
running the server in [strict mode]({{ '/docs/options#strict' | relative_url }}).

Assuming dynamic typing is enabled, some simple rules are used to determine
which datatype the field should have:

| JSON datatype               | Xapiand datatype            |
|-----------------------------|-----------------------------|
| null                        | No type                     |
| Boolean (`true` or `false`) | `boolean`                   |
| Floating point number       | `float`                     |
| Integer                     | `integer`                   |
| Object                      | `object`                    |
| Array                       | `array`                     |
| String                      | A `datetime` if the value passes [datetime detection](#datetime-detection), a `float` or `integer` if the value passes [numeric detection](#numeric-detection) a `UUID` if the value passes [uuid detection](#uuid-detection), a `text` if the value passes [text detection](#text-detection) or a `keyword`.

These are the only field datatypes that are dynamically detected. Any other
datatypes must be mapped explicitly.


## Datetime Detection

If `datetime_detection` is enabled (the default), then new string fields are
checked to see whether their contents match any of the date patterns. If a
match is found, a new datetime field is added.


### Disabling datetime detection

Dynamic datetime detection can be disabled by setting `datetime_detection` to
`false` in the index schema:

{% capture req %}

```json
PUT /my_index/

{
  "_schema": {
    "schema": {
      "datetime_detection": false
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Then, when indexing a new field that looks like a date will not be detected
as `datetime` and instead it will end up being a `keyword`:

{% capture req %}

```json
PUT /my_index/1

{
  "create": "2015/09/02"
}
```
{% endcapture %}
{% include curl.html req=req %}


## Numeric detection

While JSON has support for native floating point and integer datatypes, some
applications or languages may sometimes render numbers as strings. Usually the
correct solution is to map these fields explicitly, but numeric detection (which
is disabled by default) can be enabled to do this automatically:

{% capture req %}

```json
PUT /my_index/

{
  "_schema": {
    "schema": {
      "numeric_detection": true
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The following will detect `my_float` field to be `float` and `my_integer` to be
`integer`:

{% capture req %}

```json
PUT /my_index/1

{
  "my_float":   "1.0",
  "my_integer": "1"
}
```
{% endcapture %}
{% include curl.html req=req %}
