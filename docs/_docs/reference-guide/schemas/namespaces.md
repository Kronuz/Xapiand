---
title: Namespaces
---

In use-cases where it's not feasible/convenient to create a new field in the
schema for each field inside a tree. Namespace enabled fields allow efficient
searching of values within nested fields. For example, this feature can be used
for tags, file-system path names, or any tree-like structure for which contained
names can be many, dynamic and/or not known in advance.

The `_namespace` parameter must be enabled during the schema field creation:

{% capture req %}

```json
UPDATE /bank/1

{
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above example is the document being indexed, the parameter `_namespace`
part enables the Namespace Queries functionality.


## Partial Paths

By default namespaced fields keep information for each level in the path. This
behaviour can be modified by setting `_partial_paths` to `false`, when creating
a field schema:

{% capture req %}

```json
UPDATE /bank/1

{
  "hobbies": {
    "_namespace": true,
    "_partial_paths": false
    "Observation": {
      "Indoors": {
        "Learning": 10,
        "Reading": 7
      },
      "Outdoors": {
        "Traveling": 3
      }
    },
    "Competitive": {
      "Indoors": {
        "Boxing": 4,
        "Judo": 10
      },
      "Outdoors": {
        "Baseball": 2
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Partial paths can be used for [Namespace Queries]({{ '/docs/reference-guide/search/query-dsl/leaf-queries/namespace-queries/#partial-paths' | relative_url }}).


## Datatype

The concrete datatype for all nested objects must be of the same type as one
declared in the object enabling the `_namespace`.
