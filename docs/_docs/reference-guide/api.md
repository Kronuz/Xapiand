---
title: API Conventions
---

The Xapiand REST APIs are exposed using JSON (or MessagePack) over HTTP.

The conventions listed in here can be applied throughout the REST API, unless
otherwise specified.


## Multiple Indices

Most APIs that refer to an index parameter support execution across multiple
indices, using simple `test1,test2,test3` notation.

{% if site.serving %}
<!-- TODO: Implement feature -->
<!--
It also support  `_all` for all indices, wildcards, for example: `test*`,
`*test`, `te*t` or `*test*`, and the ability to "exclude" (-), for example:
`test*,-test3`.

All multi indices API support the following url query string parameters:

* `ignore_unavailable` - Controls whether to ignore if any specified indices are unavailable, this includes indices that don’t exist or closed indices. Either true or false can be specified.
* `allow_no_indices` - Controls whether to fail if a wildcard indices expressions results into no concrete indices. Either true or false can be specified. For example if the wildcard expression foo* is specified and no indices are available that start with foo then depending on this setting the request will fail. This setting is also applicable when _all, * or no index has been specified. This settings also applies for aliases, in case an alias points to a closed index.
* `expand_wildcards` - Controls to what kind of concrete indices wildcard indices expression expand to. If open is specified then the wildcard expression is expanded to only open indices and if closed is specified then the wildcard expression is expanded only to closed indices. Also both values (open,closed) can be specified to expand to all indices.

If none is specified then wildcard expansion will be disabled and if all is specified, wildcard expressions will expand to all indices (this is equivalent to specifying open,closed).

The defaults settings for the above parameters depend on the api being used.
-->
{% endif %}

{: .note .info}
Single index APIs such as the Document APIs and the single-index alias APIs
do not support multiple indices.
