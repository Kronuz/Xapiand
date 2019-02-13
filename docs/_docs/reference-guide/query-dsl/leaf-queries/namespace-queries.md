---
title: Namespace Queries
---

Namespace enabled fields allow efficiently searching for nested object names
in use-cases where it's not feasible/convenient to create a new field in the
schema for each name inside a tree. For example, this feature can be used for
tags, file-system path names, or any tree-like structure for which contained
names can be many, dynamic and/or not known in advance.

The `_namespace` keyword must be specified during Schema creation:

```json
{
  "style": {
    "_namespace": true,
    "_partial_paths": true,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
  },
  ...
}
```

The above example is the document being indexed, the keyword `_namespace`
enables the Namespace Queries functionality.

Searching can be done either specifying nesting field names objects or by using
[Field Expansion]({{ '/docs/reference-guide/api/#field-expansion' | relative_url }}) (joining the field
names with `.`):

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "style": {
      "clothing": "*"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "style.clothing.footwear": "casual shoes"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "style.hairstyle": "afro"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

## Skipping Fields

We can see that all nested field names are listed in the object (without
skipping any intermediate fields). If we want to skip fields, the keyword
`_partial_path` should have been be set to `true` in advance, during Schema
creation. Having make sure of that, you can perform searches like the following
(note how `"clothing"` was skipped):

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "style.footwear": "casual shoes"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Datatype

The concrete datatype for all nested objects must be of the same type as the
field with the `_namespace` keyword.
