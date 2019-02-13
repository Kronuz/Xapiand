---
title: Text Datatype
short_title: Text
---

A field to index full-text values, such as the body of an email or the
description of a product. These fields are analyzed, that is they are passed
through an analyzer to convert the string into a list of individual terms before
being indexed. The analysis process allows Xapiand to search for individual
words within each full text field. Text fields are not used for sorting and
seldom used for aggregations (although the significant text aggregation is a
notable exception).

If you need to index structured content such as email addresses, hostnames,
status codes, or tags, it is likely that you should rather use the
[Keyword Datatype](../keyword-type) instead.

By default every field in the document with text value is interpreted as
_Text Datatype_:

{% capture req %}

```json
PUT /bank/1?pretty

{
  "accountNumber": 148040,
  "balance": 1494.65,
  "employer": "Cubicide",
  "name": {
    "firstName": "Todd",
    "lastName": "Le"
  },
  "age": 29,
  "gender": "male",
  "contact": {
    "address": "409 Williams Place",
    "city": "Suitland",
    "state": "Virginia",
    "postcode": "05192",
    "phone": "+1 (830) 555-2707",
    "email": "todd.le@cubicide.net"
  },
  "checkin": {
    "_point": {
      "_longitude": -80.31727,
      "_latitude": 25.67927
    }
  },
  "favoriteFruit": "strawberry",
  "eyeColor": "brown",
  "style": {
    "_namespace": true,
    "_partial_paths": true,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
  },
  "personality": {
    "_language": "en",
    "_value": "There's a lot to say about Todd Le, but if there's anything you should know it's that he's individualistic and determined. Of course he's also charming, cheerful and precise, but far less strongly and often mixed with being grim as well. His individualism though, this is what he's pretty much loved for. Friends usually count on this and his appreciative nature especially when they need comforting or support. All in all, Todd has a fair share of lesser days too. His slyness and dominating nature sour the mood many a time and beyond what people are willing to deal with. Fortunately his determination is there to relift spirits when needed.",
    "_type": "text"
  },
  "_id": 2
}
```
{% endcapture %}
{% include curl.html req=req %}


## Stemmers

A common form of normalisation is stemming. This process converts various
different forms of words to a single form: for example, converting a plural
(e.g., _"birds"_) and a singular form of a word (_"bird"_) to the same term
(in this case, both are converted to _"bird"_).

Note that the output of a stemmer is not necessarily a valid word; what is
important is that words with closely related meaning are converted to the same
form, allowing a search to find them. For example, both the word _"happy"_ and
the word _"happiness"_ are converted to the form _"happi"_, so if a document
contained _"happiness"_, a search for _"happy"_ would find that document.

The rules applied by a stemmer are dependent on the language of the text;
Xapian includes stemmers for more than a dozen languages (and for some languages
there is a choice of stemmers), built using the **Snowball** language. We'd like to
add stemmers for more languages - see the [Snowball]{:target="_blank"}
site for information on how to contribute.

{: .note .caution }
**_Caution_**<br>
By default Xapiand doesn't do any stemming to text fields. This feature is only
enabled when the parameter `_language` (or otherwise `_stem_language`) is
specified in the Schema.


## Stem Strategy

The default `_stem_strategy` is `"stem_some"`, but you can choose others.

Other available stemming strategies are:

|---------------------------|-----------------------------------------------------------------|
| `stem_none`, `none`       | Don't perform any stemming.                                     |
| `stem_some`, `some`       | Stem all terms except for those which start with a capital letter, or are followed by certain characters (currently: `(`, `/`, `@`, `<`, `>`, `=`, `*`, `[`, `{`, `"`), or are used with operators which need positional information. _(note: stemmed terms are prefixed with 'Z')_. (This is the default mode). |
| `stem_all`, `all`         | Stem all terms _(note: no '`Z`' prefix is added)_.              |
| `stem_all_z`, `all_z`     | Stem all terms _(note: '`Z`' prefix is added)._.                |


## Stop Strategy

The default `_stop_strategy` is `"stop_stemmed"`, so stemmed forms of stopwords
aren't indexed, but unstemmed forms still are so that searches for phrases
including stop words still work.

Other available stop strategies are:

|---------------------------|-----------------------------------------------------------------|
| `stop_none`, `none`       | Don't use the stopper.                                          |
| `stop_all`, `all`         | If a word is identified as a stop word, skip it completely.     |
| `stop_stemmed`, `stemmed` | If a word is identified as a stop word, index its unstemmed form but skip the stem. Unstemmed forms are indexed with positional information by default, so this allows searches for phrases containing stopwords to be supported. (This is the default mode). |


## Parameters

The following parameters are accepted by _Text_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_language`                           | The language to use for stemming and stop words. (The default is `"none"`)              |
| `_stop_strategy`                      | The [stopper strategy]{:target="_blank"} that the stopper is going to use (defaults to `"stop_stemmed"`) |
| `_stem_language`                      | The [stemming language]{:target="_blank"} that stemming algorithm is going to use (defaults to `_language` value) |
| `_stem_strategy`                      | The [stemming strategy]{:target="_blank"} that stemming algorithm is going to use (defaults to `"stem_some"`) |
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |


[Snowball]: http://snowballstem.org
[stopper strategy]: https://xapian.org/docs/apidoc/html/classXapian_1_1TermGenerator.html#aec58751aec187d8b2647579c150667c2
[stemming language]: https://xapian.org/docs/apidoc/html/classXapian_1_1Stem.html#a6c46cedf2047b159a7e4c9d4468242b1
[stemming strategy]: https://xapian.org/docs/apidoc/html/classXapian_1_1QueryParser.html#ac7dc3b55b6083bd3ff98fc8b2726c8fd
