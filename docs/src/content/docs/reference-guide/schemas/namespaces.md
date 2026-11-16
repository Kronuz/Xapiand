---
title: "Namespaces"
---

In use-cases where it's not feasible/convenient to create a new field in the
schema for each field inside a tree. Namespace enabled fields allow efficient
searching of values within nested fields. For example, this feature can be used
for tags, file-system path names, or any tree-like structure for which contained
names can be many, dynamic and/or not known in advance.

The `_namespace` parameter must be enabled during the schema field creation:

```rest
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

The above example is the document being indexed, the parameter `_namespace`
part enables the Namespace Queries functionality.

Because the field is namespaced, every value in the tree stays searchable by its
path. Querying the full path matches the exact value:

```rest
SEARCH /bank/

{
  "_query": {
    "style.clothing.pants": "khakis"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Namespaces results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.hits.length).to.equal(10);
});
```
e2e:end -->

A path that stops short does not match: `style.clothing` is not the full path to
`khakis` (the field is `style.clothing.pants`), so this query finds nothing. What a
namespace does keep is the intermediate levels it can skip: with partial paths (on by
default, see below) `style.footwear` reaches `style.clothing.footwear`.

```rest
SEARCH /bank/

{
  "_query": {
    "style.clothing": "khakis"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Namespaces results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
  pm.expect(jsonData.hits.length).to.equal(0);
});
```
e2e:end -->

See [Namespace Queries](/Xapiand/reference-guide/search/query-dsl/leaf-queries/namespace-queries)
for the full set of ways to match namespaced paths.

## Partial Paths

By default a namespace (`_namespace: true`) keeps the information to match a path
that skips levels. This behaviour can be modified by setting `_partial_paths` to
`false`, when creating a field schema. A partial path is limited to 10 levels, and a
deeper path answers `400` (`src/database/schema.h:123`):

```rest
UPDATE /bank/1

{
  "hobbies": {
    "_namespace": true,
    "_partial_paths": false,
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

Partial paths can be used for [Namespace Queries](/Xapiand/reference-guide/search/query-dsl/leaf-queries/namespace-queries/#partial-paths).

## Datatype

The concrete datatype for all nested objects must be of the same type as one
declared in the object enabling the `_namespace`.
