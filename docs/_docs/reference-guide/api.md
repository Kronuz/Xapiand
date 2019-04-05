---
title: API Conventions
---

---

## RESTful Features

Xapiand uses a RESTful API exposed using JSON over HTTP.

When we talk about our API, we use terms like "_REST_" and "_RESTful_." "_REST_"
stands for [Representational State Transfer](https://en.wikipedia.org/wiki/Representational_state_transfer){:target="_blank"}.

The conventions listed in here can be applied throughout the REST API, unless
otherwise specified.


### RESTful HTTP Methods

You may see these standard HTTP methods referred to as CRUD, or _Create_, _Read_,
_Update_, _Delete_. Although CRUD has roots in database operations, you can also
map those operations to the standard HTTP methods. For example, use a `POST`
request to create a new resource, a `GET` request to read or retrieve a resource,
a `PATCH` or `UPDATE` request to edit a resource, and a `DELETE` request to
delete a resource.


### Deviations from REST

We do our best to use standard HTTP methods with accurate and well-known status
codes in the Xapiand API, but here are some additions and deviations.

Additionally to the standard HTTP methods, we also use other custom methods
such as `UPDATE` and `STORE`, for certain operations.


---

## HTTP Methods

- **_Safe Methods_**
  Requests that use _safe_ HTTP methods won't alter a resource at all. Examples
  are `GET`, `OPTIONS` and `HEAD`.

- **_Idempotent Methods_**
  An _idempotent_ HTTP method is a HTTP method that can be called many times
  without different outcomes. It would not matter if the method is called only
  once, twice or a hundred times over, the result should be the same. This only
  applies to the result, not the resource itself. Examples are `DELETE`, `PUT`.

- **_Not Safe/Idempotent Methods_**
  This are requests that will alter the resource *and* potentially end up with
  different results every time. Examples are `POST`.


### Custom Methods

The _Standard Methods_, the ones we all are familiar with, have simpler and
well-defined semantics but there is functionality that cannot be easily
expressed via standard methods. Custom methods refer to such API methods.

Simply pass the required method as a non-standard HTTP method in the request.

Example:

{% capture req %}

```json
INFO /some/resource/name
```
{% endcapture %}
{% include curl.html req=req %}

If your firewall rules don't support non-standard HTTP methods like `PATCH`,
`UPDATE`, `STORE` or `DELETE`, for example, you have two options:

+ Use HTTP Method Override
+ Use HTTP Method Mapping


#### HTTP Method Override

You can use the [X-HTTP-Method-Override](http://www.hanselman.com/blog/HTTPPUTOrDELETENotAllowedUseXHTTPMethodOverrideForYourRESTServiceWithASPNETWebAPI.aspx){:target="_blank"}
(or _HTTP-Method-Override_) header. Pass the method you want to use in the
`X-HTTP-Method-Override` header and use the `POST` method.

Example:

{% capture req %}

```json
POST /some/resource/name
X-HTTP-Method-Override: INFO
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
The Method Override won't work with any other method other than `POST`, you'll
receive an error.


#### HTTP Method Mapping

Custom methods can use the following generic HTTP mapping:
`http://service.name:8880/some/resource/name:customMethod`

To use method mappings pass the mapping in the URL and use HTTP `POST` verb
since it has the most flexible semantics, except for methods serving as an
alternative get or list which may use `GET`.

Example:

{% capture req %}

```json
GET /some/resource/name:info
```
{% endcapture %}
{% include curl.html req=req %}


---

## Resource Paths

### To slash or not to slash

That is the question we hear often. Onward to the answers! Historically,
it's common for URLs _with a trailing slash_ to indicate a _directory_, and
those _without a trailing slash_ to denote a _file_:

+ `http://example.com/foo` (without trailing slash, conventionally a file)
+ `http://example.com/foo/` (with trailing slash, conventionally a directory)

Source: [Google WebMaster Central Blog - To slash or not to slash](https://webmasters.googleblog.com/2010/04/to-slash-or-not-to-slash.html){:target="_blank"}


### Trailing slashes are important

To us, trailing slashes are important to distinguish between a path to an
_Index_ (a _directory_) and a path to a _Document_ (a _file_).

The following will delete a _single document_ from index `/some/resource/name`,
the document with ID `name`:

{% capture req %}

```json
DELETE /some/resource/name
```
{% endcapture %}
{% include curl.html req=req %}

Whilst the next example will delete the whole index `/some/resource/path/` with
_all its documents_ in it as well:

{% capture req %}

```json
DELETE /some/resource/path/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
**_Remember_**<br>
Trailing slashes in resource paths are important, always make sure you are
requesting a method for the proper resource path.
**Trailing slashes _do mean something_**.


---

## JSON and MessagePack

The Xapiand API can process JSON objects or MessagePack objects.

{: .note .tip }
**_MessagePack_**<br>
[MessagePack](https://msgpack.org){:target="_blank"} is more efficient and it
is our internal representation of the data.

### Deviations from JSON

#### Comments

JSON can have C-style `/* */` block or single line `//` comments. Comments are
allowed everywhere in the JSON document.

#### Trailing Commas

JSON can have trailing commas.


---

## Field Expansion

JSON or MessagePack fields in objects passed to Xapiand are expanded.
For example, the following nested object:

```json
{
  "contact": {
    "address": {
      "country": {
        "name": "Italy"
      }
    }
  }
}
```

Is equivalent to:

```json
{
  "contact.address.country.name": "Italy"
}
```


---

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

{: .note .info }
Single index APIs such as the Document APIs and the single-index alias APIs
do not support multiple indices.
