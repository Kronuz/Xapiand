---
title: "Scripting"
---

The scripting functionality lets you run small scripts to compute custom
expressions: derive a field at index time, normalize a value on write, keep a
counter, or evaluate a custom score for a query.

The scripting language is [Lua](https://www.lua.org/), embedded via
[sol2](https://github.com/ThePhD/sol2). Scripts run inside the request, with the
document available as the `_doc` object.

For example, this indexes a document and computes a `full_name` field from two
others in the same request (the `?commit` makes the write immediately visible so
we can read it back):

```rest
PUT /twitter/user/Ada?commit

{
  "first_name": "Ada",
  "last_name": "Lovelace",
  "_script": "_doc.full_name = _doc.first_name .. ' ' .. _doc.last_name"
}
```

:::hint[Dot Access Notation]{.tip}
When accessing document objects in the scripts, you can either use _dot access_
notation as exemplified above (i.e. `_doc.full_name`) or _array call_ notation
(i.e. `_doc["full_name"]`).
:::

The stored document now carries the derived field:

```rest
GET /twitter/user/Ada
```

<!-- e2e:begin
```js
pm.test("full_name was computed by the script", function () {
  pm.expect(pm.response.json().full_name).to.eql("Ada Lovelace");
});
```
e2e:end -->

## How to Use Scripts

Wherever scripting is supported in the Xapiand API, the structure for scripts
follows the same pattern. The short form is a string with the script body:

```json
"_script": "<script_name|script_body>"
```

The long form is an object under the `_lua` key:

```json
"_script": {
  "_type": "script",
  "_lua": {
    ( "_name": "<script_name>", )?
    ( "_body": "<script_body>", )?
    ( "_params": <params>, )?
  }
}
```

### Script Caching

All scripts are cached so that they only need to be recompiled when updates
occur. By default, the cache size is 100 and scripts do not have a time-based
expiration.

:::hint[Prefer Parameters]{.tip}
The first time Xapiand sees a new script, it compiles it and stores the compiled
version in a cache. Compilation can be a **heavy process**, so pass values in as
_Variables_ (parameters) instead of hard-coding them, and reuse _Foreign
Scripts_ where you can.
:::

### Variables

Xapiand adds a few default variables to the running script context:

| Variable       | Description                                                  |
|----------------|--------------------------------------------------------------|
| `_doc`         | Current document.                                            |
| `_old_doc`     | Old document (in case of updates / deletes).                 |
| `_method`      | HTTP method that triggered the script.                       |

If you need to pass additional values into a script, pass them as named
parameters instead of hard-coding them. That way the compiled script stays in
the cache and is reused across requests. For example, to multiply a field by a
configurable factor, pass the factor in `_params` rather than baking it into the
body:

```rest
PUT /twitter/user/Grace?commit

{
  "multiplied_field": 7,
  "_script": {
    "_type": "script",
    "_lua": {
      "_body": "_doc.multiplied_field = _doc.multiplied_field * multiplier",
      "_params": {
        "multiplier": 3
      }
    }
  }
}
```

```rest
GET /twitter/user/Grace
```

<!-- e2e:begin
```js
pm.test("multiplied_field = 7 * 3", function () {
  pm.expect(pm.response.json().multiplied_field).to.eql(21);
});
```
e2e:end -->

The body is compiled once; only the parameter changes between requests.

### Isolation and Limits

Every call runs in its own environment: `_doc`, `_old_doc`, `_method`, the
parameters and every global the script sets live for that call only, and writes
into `string`, `table`, `math` and `utf8` stay in the call. Two requests running
the same script at once never see each other's data. Only the base libraries,
`string`, `table`, `math` and `utf8` are available (no `io`, `os`, `load`,
`dofile` or `collectgarbage`). Also absent, because their cost cannot be bounded:
`string.pack`, `string.unpack`, `string.packsize`, `string.dump`, to-be-closed
variables (`<close>`), and the `__gc`, `__close` and `__mode` metamethods (weak
tables). `print` writes to the server log, at most 4 KiB per call.

A script shares its event loop with other requests: it runs in slices and yields
between them. A call that crosses a bound ends with `400 Bad Request` and the
document is not indexed:

| Bound                | Value                                                                    |
|----------------------|--------------------------------------------------------------------------|
| Instructions         | 100,000,000 per call                                                     |
| Work                 | 500,000,000 units per call: the bytes, pattern steps and table elements the library calls touch, about a nanosecond each |
| Memory               | 64 MiB of live data per compiled script on each thread                   |
| Pattern matching     | `max(1,000,000, 64 x subject length)` steps per `string` call            |
| Callbacks            | 1,000,000 instructions in a row inside a callback that cannot yield (a `gsub` function, an `__lt` run by `table.sort`) |

Work is charged by what a call touches, not by how often it is made: a plain
`find` over a megabyte costs a megabyte, a pattern costs its steps, and `rep`,
`sub`, `upper`, `reverse`, `format`, `..`, string comparison, `utf8.*`,
`table.concat`, `unpack`, `insert`, `remove` and `move` cost their lengths. A
call that has done more than a slice of it yields at its next instruction.

The document a script leaves in `_doc` is converted back with a budget of its
own: at most 16 MiB of string and key bytes and 262,144 value units (a scalar is 1, an
array 2, an object 8), counting every reference to a shared string or table again. A script that returns more answers
`413 Payload Too Large`. Any other error a script raises answers `400 Bad
Request` with the script's message (cut at 1 KiB), and the document is not
indexed.

A script cannot catch the bound errors with `pcall` or `xpcall`, whatever its
error handlers do.

## Real-World Examples

### Normalize on write

Clean up input as it comes in, for example lower-casing an email so lookups are
case-insensitive, using Lua's string library:

```rest
PUT /twitter/user/Alan?commit

{
  "email": "Alan@Example.COM",
  "_script": "_doc.email = string.lower(_doc.email)"
}
```

```rest
GET /twitter/user/Alan
```

<!-- e2e:begin
```js
pm.test("email was normalized to lower case", function () {
  pm.expect(pm.response.json().email).to.eql("alan@example.com");
});
```
e2e:end -->

### Fill in a default

Set a field only when it's missing, leaving it untouched when the client
provides one:

```rest
PUT /twitter/user/Edsger?commit

{
  "title": "engineer",
  "_script": "if _doc.status == nil then _doc.status = 'active' end"
}
```

```rest
GET /twitter/user/Edsger
```

<!-- e2e:begin
```js
pm.test("default status was applied", function () {
  pm.expect(pm.response.json().status).to.eql("active");
});
```
e2e:end -->

### Maintain a counter

Use `_old_doc` to read the previous value and bump it on every update. First
index the base document:

```rest
PUT /twitter/user/John

{
  "serial": 1,
  "name": "John"
}
```

Then increment the counter on update; the script reads the old value and writes
the new one:

```rest
PUT /twitter/user/John?commit

{
  "name": "John",
  "_script": "_doc.serial = _old_doc.serial + 1"
}
```

```rest
GET /twitter/user/John
```

<!-- e2e:begin
```js
pm.test("serial was incremented", function () {
  pm.expect(pm.response.json().serial).to.eql(2);
});
```
e2e:end -->

## Foreign Scripts

Scripts can also be loaded from a different database / document. These are called
_foreign scripts_. To use a foreign script, first create a document containing
the script:

```rest
PUT /path/to/my_scripts/multiplier?commit

{
  "_recurse": false,
  "script": {
    "_lua": {
      "_body": "_doc.multiplied_field = _doc.multiplied_field * multiplier",
      "_params": {
        "multiplier": 1
      }
    }
  }
}
```

We place the script inside the `"script"` field and set `"_recurse": false` so
`"script"` isn't recursed and analyzed as a regular field by the Schema.

We then use the foreign script by pointing `_foreign` at the script document (its
full index path and document ID). Xapiand automatically reads that document's
`"script"` field (the same convention as _foreign schemas_), so you don't add a
selector yourself. Here the index is `path/to/my_scripts` and the document ID is
`multiplier`. The `_params` passed at the call site override the script's own
defaults, so `multiplier` is `3` rather than the stored `1`:

```rest
PUT /twitter/user/Katherine?commit

{
  "multiplied_field": 7,
  "_script": {
    "_type": "foreign/object",
    "_foreign": "path/to/my_scripts/multiplier",
    "_params": {
      "multiplier": 3
    }
  }
}
```

```rest
GET /twitter/user/Katherine
```

<!-- e2e:begin
```js
pm.test("foreign script multiplied the field", function () {
  pm.expect(pm.response.json().multiplied_field).to.eql(21);
});
```
e2e:end -->
