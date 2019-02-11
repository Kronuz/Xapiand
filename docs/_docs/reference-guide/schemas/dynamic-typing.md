---
title: Dynamic Typing
---

{: .note .construction }
_This section is a **work in progress**..._

By default, when a previously unseen field is found in a document,
{{ site.name }} will add the new field to the schema. This behaviour can be
disabled globally by running the server in _strict_ mode.

Assuming dynamic typing is enabled, some simple rules are used to determine
which datatype the field should have:

## Date detection

If date_detection is enabled (default), then new string fields are checked to
see whether their contents match any of the date patterns. If a match is found,
a new date field is added.

### Disabling date detection

Dynamic date detection can be disabled by setting `date_detection` to `false`

```json
PUT my_index/:schema

{
  "date_detection": false
}
```

```json
PUT my_index/1

{
  "create": "2015/09/02"
}
```
