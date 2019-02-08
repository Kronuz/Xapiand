---
title: API Conventions
---

## RESTful Features of the Xapiand API

Xapiand uses a RESTful API exposed using JSON (or MessagePack) over HTTP.

When we talk about our API, we use terms like "_REST_" and "_RESTful_." "_REST_"
stands for [Representational State Transfer](https://en.wikipedia.org/wiki/Representational_state_transfer){:target="_blank"}.

The conventions listed in here can be applied throughout the REST API, unless
otherwise specified.

### RESTful HTTP Methods

You may see these standard HTTP methods referred to as CRUD, or _Create_, _Read_,
_Update_, _Delete_. Although CRUD has roots in database operations, you can also
map those operations to the standard HTTP methods. For example, use a _POST_
request to create a new resource, a _GET_ request to read or retrieve a resource,
a _PATCH_ request to edit a resource, and a _DELETE_ request to delete a resource.

### Deviations from REST

We do our best to use standard HTTP methods with accurate and well-known status
codes in the Xapiand API, but here are some additions and deviations.

Additionally to the standard HTTP methods, we also use _MERGE_ and _STORE_
methods for certain operations.

### HTTP methods and response codes

- **GET**, **OPTIONS** and **HEAD** requests are safe and idempotent, and won't alter a resource.
- **DELETE**, **PUT**, **MERGE** and **STORE** methods are idempotent.
- **POST** and **PATCH** aren't safe or idempotent.

{: .note .info}
**_Idempotent methods_**<br>
An _idempotent_ HTTP method is a HTTP method that can be called many times
without different outcomes. It would not matter if the method is called only
once, twice or a hundred times over, the result should be the same. This only
applies to the result, not the resource itself.

If your firewall rules don't support HTTP methods like _PATCH_, _MERGE_, _STORE_
or _DELETE_, use the [X-HTTP-Method-Override](http://www.hanselman.com/blog/HTTPPUTOrDELETENotAllowedUseXHTTPMethodOverrideForYourRESTServiceWithASPNETWebAPI.aspx){:target="_blank"} (or _HTTP-Method-Override_) header. Pass the method you want to use in the
`X-HTTP-Method-Override` header and make your call using the POST method. The
override won't work with any other method, so if you try and use the override
header with a _GET_, _PATCH_, _MERGE_, _STORE_, _PUT_, or _DELETE_ method,
you'll receive an error.


## JSON and MsgPack

The Xapiand API can process JSON objects or MsgPack objects, MsgPack being more
efficient as it is the internal representation of the data.

### Deviations from JSON

#### Comments

JSON can have C-style `/* */` block or single line `//` comments. Comments are
allowed everywhere in the JSON document.

#### Trailing Commas

JSON can have trailing commas.

#### Hex Codes

JSON Accepts binary codes as escaped `"\xHH"` in strings.


## Multiple Indices

Most APIs that refer to an index parameter support execution across multiple
indices, using simple `test1,test2,test3` notation.

{% if site.serving %}
<!-- TODO: Unimplemented Feature! -->
<!--
It also support  `_all` for all indices, wildcards, for example: `test*`,
`*test`, `te*t` or `*test*`, and the ability to "exclude" (-), for example:
`test*,-test3`.

All multi indices API support the following url query string parameters:

* `ignore_unavailable` - Controls whether to ignore if any specified indices are unavailable, this includes indices that don't exist or closed indices. Either true or false can be specified.
* `allow_no_indices` - Controls whether to fail if a wildcard indices expressions results into no concrete indices. Either true or false can be specified. For example if the wildcard expression foo* is specified and no indices are available that start with foo then depending on this setting the request will fail. This setting is also applicable when _all, * or no index has been specified. This settings also applies for aliases, in case an alias points to a closed index.
* `expand_wildcards` - Controls to what kind of concrete indices wildcard indices expression expand to. If open is specified then the wildcard expression is expanded to only open indices and if closed is specified then the wildcard expression is expanded only to closed indices. Also both values (open,closed) can be specified to expand to all indices.

If none is specified then wildcard expansion will be disabled and if all is specified, wildcard expressions will expand to all indices (this is equivalent to specifying open,closed).

The defaults settings for the above parameters depend on the api being used.
-->
{% endif %}

{: .note .info}
Single index APIs such as the Document APIs and the single-index alias APIs
do not support multiple indices.
