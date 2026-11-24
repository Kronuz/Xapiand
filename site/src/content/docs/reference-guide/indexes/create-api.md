---
title: "Create Index API"
---

The _Create Index API_ is used to manually create or update an index in Xapiand.

All documents in Xapiand are stored inside of one index or another, indexes
are usually dynamically created, but certain settings can only be set for
new indexes.

This creates an index named `twitter/tweet` with all default setting:

```rest
PUT /twitter/tweet/
```

:::hint{.warning}
`PUT /twitter/tweet/` is not the same as `PUT /twitter/tweet`, the former creates index
`/twitter/tweet/` and the later adds document `twitter/tweet` to index `/`.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

### Index Names

An index name is a path: segments separated by `/` (see
[Index](/Xapiand/basics#index)). What a name cannot hold:

- Bytes that are not well-formed UTF-8, NUL, control characters (below U+0020 and DEL)
  and `\`: the request answers `400`.
- A path that is empty or that, once `..` and `.` segments are resolved, is outside the
  data directory: `400`.
- About 240 bytes or more in all (bytes, so multi-byte characters count faster): the
  index's entry in `.xapiand/indexes` is a keyword term, which is at most 245 bytes
  with its prefix, and the request answers `400` "Keyword too long".

Spaces, dots and `:` are accepted. The characters `*`, `?` and `[` are accepted in a
name too, but a [search](/Xapiand/reference-guide/api#multiple-indexes) reads them as a
pattern, so an index named with them cannot be searched by name.

```rest
PUT /createapi/back%5Cslash/
```

<!-- e2e:begin
---
description: Create: a backslash is refused
---

```js
pm.test("A backslash is not allowed in an index name", function () {
  pm.response.to.have.status(400);
});
```
e2e:end -->

Indexes can be paths (including `/` as part of the path).

## Settings

Each index created can have specific settings associated with it, such can be
defined in the body (these can only be set on a **new** index, so this example
uses a fresh one):

```rest
PUT /twitter/tweet_sharded/

{
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  }
}
```

The default for `number_of_shards` is `5`; the above command creates an index
with `3` shards. `number_of_replicas` is accepted and stored, but has no effect
on a single node. A new index created with `_strict` must name both settings:
without either one the write answers `400` ("Value of 'number_of_shards' is
missing", `src/index_resolver_lru.cc`).

:::hint[Only for New Indexes]{.info}
`number_of_shards` can be specified like this only for new indexes and any
attempt to change the value during an update will result in a Bad Request error.
:::

## Schemas

The _Create Index API_ also allows to provide a schema, also to be defined in
the body:

```rest
PUT /twitter/tweet/

{
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

More information about Schemas can be found [here](/Xapiand/reference-guide/schemas).

:::hint[Only for New Indexes]{.info}
Some configurations about schemas can be set only the first time a field is
created (`_type`, for example) and any attempt to change the values during an
update will result in a Bad Request error.
:::
