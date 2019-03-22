---
title: Create Index API
short_title: Create API
---

The _Create Index API_ is used to manually create an index in Xapiand. All
documents in Xapiand are stored inside of one index or another.

{% capture req %}

```json
POST /twitter/:touch?pretty
```
{% endcapture %}
{% include curl.html req=req %}

This create an index named twitter with all default setting.


## Index name limitations

There are several limitations to what you can name your index. The complete
list of limitations is:

- Cannot be `.` or `..`
- Cannot start with `:`
- Connot contain `.` (except for the first character)
- Full path cannot be longer than 243 bytes (note it is bytes, so multi-byte
  characters will count towards the 255 limit faster)

Indices can be paths (including `/` as part of the path).


## Index Settings

Each index created can have specific settings associated with it, such can be
defined in the body:

{% capture req %}

```json
POST /twitter/:touch?pretty

{
  "_settings": {
    "shards": 3,
    "replicas": 2
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Index Schema

The Create Index API also allows to provide a schema, also to be defined in the
body:

{% capture req %}

```json
POST /twitter/:touch?pretty

{
  "_schema": {
    "schema": {
      "name": {
        "_type": "text"
      },
      "age": {
        "_type": "positive"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}




{% if site.serving %}

---

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

# Future

{% capture req %}

```json
UPDATE /twitter/?pretty

{
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  },
  "_schema": {
    "name": {
      "_type": "text"
    },
    "age": {
      "_type": "positive"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

---

{% capture req %}

```json
UPDATE /twitter/?pretty

{
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  },
  "_schema": {
    "_type": "foreign/object"
    "_endpoint": ".schemas/object.schema"
    "name": {
      "_type": "text"
    },
    "age": {
      "_type": "positive"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% endif %}
