---
title: Stop Words
---

Xapian supports a stop word list, which allows you to specify words which should be removed from a query before processing. This list can be overridden within user search, so stop words can still be searched for if desired:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "these days are few and far between"
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
      ...
      {
      "city": "Grandview",
      "gender": "F",
      "balance": 1408.47,
      "firstname": "Yvonne",
      "lastname": "Day",
      "employer": "Talkalot",
      "favoriteFruit": "strawberry",
      "eyeColor": "brown",
      "phone": "+1 (955) 522-3379",
      "state": "Nebraska",
      "account_number": 101121,
      "address": "227 Cherry Street",
      "age": 33,
      "email": "yvonne.day@talkalot.com",
      "_id": 51,
      "personality": "A lot can be assumed when you first see Yvonne Day, but most know that above all else he's clear-headed and elegant. Of course he's also persuasive, honorable and kind, but they're in shorter supply, especially considering they're mixed with being irrational as well. His clear-headedness though, this is what he's often admired for. Friends usually count on this and his playfulness whenever they need help. Nobody's perfect of course and Yvonne has plenty of lesser desired aspects too. His disloyalty and desperation sour the mood many a time even at the best of times. Fortunately his elegance helps keep them in check for at least a little.",
      "#docid": 39,
      "#rank": 0,
      "#weight": 2.5100731343066875,
      "#percent": 100
      },
      ...
    ]
  },
  ...
}
```

We can look this individual document, the field personality do not include the entire phrase we look in the query that is because remove all the stop words and only use "days" for the query.


## Disable Stop Words

By default the stop words are used in every field text, if you want to disable the stop words you need to use `_stopword` keyword to false in the field with the text at the time to index the document:

{% capture req %}

```json
PUT /bank/1?pretty

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
  "personality": {
      "_stopword": false,
      "_value": "A lot can be said of Jocelyn Martinez, but most know that above all else she is responsive and passionate, connect with the people. Of course she is also sentimental, dynamic and flexible, but they are often slightly tainted by a mindset of being malicious as well. Her responsiveness though, this is what she is pretty much known for. People regularly count on this and her capabilities when they are feeling down. Nobody is perfect of course and Jocelyn has a share of darker sides to deal with too. Her morbid nature and ignorance are far from ideal, though more on a personal level than for others. Fortunately her passion is usually there to soften the blows. Simply there is not a bad day for him"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}

Stop words can be searched adding `+` to the stop word desired or using `_no_stopwords` keyword:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "+these days +are +few +and +far +between"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above example is equivalent to:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_no_stopwords": {
      "personality":  "these days are few and far between"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}