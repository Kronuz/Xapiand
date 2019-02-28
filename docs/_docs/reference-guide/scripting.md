---
title: Scripting
---

The scripting module enables you to use scripts to evaluate custom expressions.
For example, you could use a script to return "script fields" as part of a
search request or evaluate a custom score for a query.

The scripting language currently supported is [ChaiScript](http://chaiscript.com).

For example, the following script is used while adding/updating a given document
to atomically increment a "serial" number field:

{% capture req %}

```json
PUT /customer/1?pretty

{
  "serial": 1,
  "_script": "doc.serial = (old ? old.serial + 1 : doc.serial)"
}
```
{% endcapture %}
{% include curl.html req=req %}


## How to Use Scripts

Wherever scripting is supported in the Xapiand API, the syntax follows the same
pattern:

```json
{
  "_type": "script",
  "_name": "<script_name>",
  "_value": "<script_body>",
}
```


{: .note .tip }
**_Prefer Parameters_**<br>
The first time Xapiand sees a new script, it compiles it and stores the compiled
version in a cache. Compilation can be a heavy process.


### Variables

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

If you need to pass variables into the script, you should pass them in as named
params instead of hard-coding values into the script itself. For example, if you
want to be able to multiply a field value by different multipliers, don't
hard-code the multiplier into the script:

```json
  "_script": {
    "_type": "script",
    "_value": "doc['my_field'] * 2"
  }
```

Instead, pass it in as a named parameter:

```json
  "_script": {
    "_type": "script",
    "_value": "doc['my_field'] * multiplier"
    "_params": {
      "multiplier": 2
    }
  }
```

The first version has to be recompiled every time the multiplier changes. The
second version is only compiled once.


### Script Caching

All scripts are cached by default so that they only need to be recompiled when
updates occur.

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

By default, scripts do not have a time-based expiration, but you
can change this behavior by using the `script.cache.expire` setting. You can
configure the size of this cache by using the script.cache.max_size setting. By
default, the cache size is 1000.
