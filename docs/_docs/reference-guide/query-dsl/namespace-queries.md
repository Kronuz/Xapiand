---
title: Namespace Query
---

The `_namespace` keyword allow search nested objects fields like tags.

{% capture req %}

```json
PUT /bank/1?pretty

{
  "accountNumber": 121931,
  "balance": 221.46,
  "employer": "Globoil",
  "name": {
    "firstName": "Juan",
    "lastName": "Ash"
  },
  "age": 24,
  "gender": "male",
  "contact": {
    "address": "630 Victor Road",
    "city": "Leyner",
    "state": "Indiana",
    "phone": "+1 (924) 594-3216",
    "email": "juan.ash@globoil.co.uk"
  },
  "favoriteFruit": "raspberry",
  "eyeColor": "blue",
  "style": {
    "_namespace": true,
    "_partial_paths": true,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
  },
  "personality": "It's hard to describe..."
}
```
{% endcapture %}
{% include curl.html req=req %}

The above example is the document indexed, the keyword `_namespace` allow nested fields search joined with `.`:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": { "style.clothing": "*" }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": { "style.clothing.footwear": "casual shoes" }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": { "style.hairstyle": "afro" }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can see that the nested object must be in the same order without skip a intermediate field. If we want to skip any field in the middle the keyword `_partial_path`must be set to true and you can perform searches like this:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": { "style.footwear": "casual shoes" }
}
```
{% endcapture %}
{% include curl.html req=req %}