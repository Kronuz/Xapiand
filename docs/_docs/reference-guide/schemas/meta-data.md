---
title: Schema Metadata
---


A Schema can have custom meta data associated with it. These are not used at all
by Xapiand, but can be used to store application-specific metadata, such as the
class that a document belongs to or schema descriptions.

The `_meta` field can be updated on an existing type using the
[Create Index API]({{ '/docs/reference-guide/indices/create-api' | relative_url }}):

{% capture req %}

```json
PUT /my_index/

{
  "_schema": {
    "_meta": {
      "description": "Schema description here",
      "class": "MyApp::User",
      "version": {
        "min": "1.0",
        "max": "1.3"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

This `_meta` info can be retrieved with the
[Get Index API]({{ '/docs/reference-guide/indices/get-api' | relative_url }}):


{% capture req %}

```json
GET /my_index/._schema._meta
```
{% endcapture %}
{% include curl.html req=req %}
