---
title: Create Index API
short_title: Create API
---

The _Create Index API_ is used to manually create an index in Xapiand. All
documents in Xapiand are stored inside of one index or another.

{% capture req %}

```json
PUT /twitter/
```
{% endcapture %}
{% include curl.html req=req %}

This create an index named twitter with all default setting.

{: .note .warning }
`PUT /twitter/` is not the same as `PUT /twitter`, the former creates index
`/twitter/` and the later adds document `twitter` to index `/`.
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).


### Index Name Limitations

There are several limitations to what you can name your index. The complete
list of limitations is:

- Cannot be `.` or `..`
- Cannot include `\`, `*`, `?`, `"`, `<`, `>`, `|`, ` ` (space character), `,`, `#`, `:`
- Cannot include `/` (as part of the name, it's used for paths)
- Connot include `.` (except for the first character)
- Full path cannot be longer than 243 bytes (note it is bytes, so multi-byte
  characters will count towards that limit faster)

Indices can be paths (including `/` as part of the path).


## Settings

Each index created can have specific settings associated with it, such can be
defined in the body:

{% capture req %}

```json
PUT /twitter/

{
  "_settings": {
    "number_of_shards": 3,
    "number_of_replicas": 2
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The default for `number_of_shards` is `5`, and the default for
`number_of_replicas` is `1` (ie one replica for each primary shard), the above
command creates an index with `3` shards and `2` replicas.

{: .note .info }
**_New Indexes Only_**
`number_of_shards` and `number_of_replicas` can be specified like this only
for new indexes.


## Schemas

The _Create Index API_ also allows to provide a schema, also to be defined in
the body:

{% capture req %}

```json
PUT /twitter/

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
