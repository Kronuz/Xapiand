---
title: Simple Query
---

This is the standard query for performing text queries, if you use `_language` keyword at the index time a stemming algorithm is used which is a process of linguistic normalisation, in which the variant forms of a word are reduced to a common form. This query example match with any document with connection in the text field personality but also match with connect because connection is reduced to connect by the stemmig algorithm. By default the `_language` is empty and in that case the stemming is not used.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "responsive"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Resulting in:

```json
{
  ...
  "#query": {
    "#total_count": 2,
    "#matches_estimated": 2,
    "#hits": [
      {
        "city": "Leyner",
        "gender": "M",
        "balance": 3221.46,
        "firstname": "Jocelyn",
        "lastname": "Martinez",
        "employer": "Globoil",
        "favoriteFruit": "banana",
        "eyeColor": "blue",
        "phone": "+1 (924) 594-3216",
        "state": "Indiana",
        "account_number": 121931,
        "address": "630 Victor Road",
        "age": 24,
        "email": "jocelyn.martinez@globoil.co.uk",
        "_id": 1,
        "personality": "Plenty of people...",
        "#docid": 15,
        "#rank": 0,
        "#weight": 4.180148745722705,
        "#percent": 100
      },
      {
        "city": "Suitland",
        "gender": "M",
        "balance": 2494.65,
        "firstname": "Dennis",
        "lastname": "Steele",
        "employer": "Cubicide",
        "favoriteFruit": "banana",
        "eyeColor": "brown",
        "phone": "+1 (830) 555-2707",
        "state": "Virginia",
        "account_number": 148040,
        "address": "409 Williams Place",
        "age": 29,
        "email": "dennis.steele@cubicide.net",
        "_id": 2,
        "personality": "Many things can be said...",
        "#docid": 35,
        "#rank": 1,
        "#weight": 4.166116268253688,
        "#percent": 99
      }
    ]
  }, ...
}
```

We can see in the result that just one document contain the word connection in the text and the other just the word connect in personality field and both match due the stemming.
