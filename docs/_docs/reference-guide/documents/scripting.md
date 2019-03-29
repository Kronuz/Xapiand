---
title: Scripting
---

The scripting functionality enables you to use scripts to evaluate custom
expressions. For example, you could use a script to return "script fields" as
part of a search request or evaluate a custom score for a query.

The scripting language currently supported is
[ChaiScript](http://chaiscript.com){:target="_blank"}.

For example, the following script is used while adding/updating a given document
to increment a "serial" number field:

{% capture req %}

```json
PUT /customer/1

{
  "_script": "_doc.serial = _old_doc.serial + 1"
}
```
{% endcapture %}
{% include curl.html req=req %}

The above example initializes the first serial number of the document to 1 and
increments the counter by one thereafter.

{: .note .tip }
**_Dot Access Notation_**<br>
When accessing document objects in the scripts, you can either use _dot access_
notation as exemplified above (i.e. `_doc.serial`) or _array call_ notation
(i.e. `_doc["serial"]`).


## How to Use Scripts

Wherever scripting is supported in the Xapiand API, the structure for scripts
follows the same patterns:

```json
"_script" {
  "_type": "script",
  "_chai": {
    ( "_name": "<script_name>", )?
    ( "_body": "<script_body>", )?
    ( "_params": <params>, )?
  }
}
```

Or, for short:

```json
"_script": "<script_name|script_body>"
```


### Script Caching

All scripts are cached so that they only need to be recompiled when updates
occur. By default, the cache size is 100 and scripts do not have a time-based
expiration.

{: .note .tip }
**_Prefer Parameters_**<br>
The first time Xapiand sees a new script, it compiles it and stores the compiled
version in a cache. Compilation can be a **heavy process**, so try using
_Foreign Scripts_ and _Variables_.


### Variables

Xapiand adds a few default variables to the running script context:

| Variable       | Description                                                  |
|----------------|--------------------------------------------------------------|
| `_doc`         | Current document.                                            |
| `_old_doc`     | Old document (in case of updates / deletes).                 |
| `_method`      | HTTP method that triggered the script.                       |

If you need to pass additional variables into the script, you should pass them
in as named parameters instead of hard-coding values into the script itself.
For example, if you want to be able to multiply a field value by different
multipliers, don't hard-code the multiplier into the script:

{% capture req %}

```json
PUT /customer/1

{
  "multiplied_field": 7,
  "_script": {
    "_type": "script",
    "_value": "_doc.multiplied_field *= 2"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Instead, pass it in as a named parameter:

{% capture req %}

```json
PUT /customer/1

{
  "multiplied_field": 7,
  "_script": {
    "_type": "script",
    "_value": "_doc.multiplied_field *= multiplier",
    "_params": {
      "multiplier": 2
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The first version has to be recompiled every time the multiplier changes. The
second version is only compiled once.


### Foreign Scripts

Scripts can also be loaded from a different database / document. These are
called _foreign scripts_. To use foreign script, first you need to create a
document containing the script:

{% capture req %}

```json
PUT /path/to/my_scripts/multiplier

{
  "_recurse": false,
  "script": {
    "_chai": {
      "_body": "_doc.multiplied_field *= multiplier",
      "_params": {
        "multiplier": 1
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We're placing the script inside the `"script"` field, but we also use
`"_recurse": false` so `"script"` doesn't get recursed and analyzed as a regular
field by the Schema.

We then can use the foreign script by specifying the `_endpoint` (i.e. the full
index path, the document ID and the selector). In the example, the index is
`path/to/my_scripts`, the document ID is `multiplier` and the selector is a
[Drill Selector]({{ '/docs/exploration#drill-selector' | relative_url }}) that
gets the `"script"` field:

{% capture req %}

```json
PUT /customer/1

{
  "multiplied_field": 7,
  "_script": {
    "_type": "foreign/object",
    "_endpoint": "path/to/my_scripts/multiplier.script",
    "_params": {
      "multiplier": 3
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

