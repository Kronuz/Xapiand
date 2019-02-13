---
title: Text Datatype
short_title: Text
---

A field to index full-text values, such as the body of an email or the description of a product. These fields are analyzed, that is they are passed through an analyzer to convert the string into a list of individual terms before being indexed. The analysis process allows Xapiand to search for individual words within each full text field. Text fields are not used for sorting and seldom used for aggregations (although the significant text aggregation is a notable exception).

If you need to index structured content such as email addresses, hostnames, status codes, or tags, it is likely that you should rather use a keyword field.

By default every field in the document with text value is interpreted as type `text`:

{% capture req %}

```json
PUT /bank/1:search?pretty

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
      "_value": "Theres a lot to say about Todd Le, but if theres anything you should know its that hes individualistic and determined. Of course hes also charming, cheerful and precise, but far less strongly and often mixed with being grim as well. His individualism though, this is what hes pretty much loved for. Friends usually count on this and his appreciative nature especially when they need comforting or support. All in all, Todd has a fair share of lesser days too. His slyness and dominating nature sour the mood many a time and beyond what people are willing to deal with. Fortunately his determination is there to relift spirits when needed.",
      "_type": "text"
    }
  }
```
{% endcapture %}
{% include curl.html req=req %}

## Stemmers

A common form of normalisation is stemming. This process converts various different forms of words to a single form: for example, converting a plural (e.g., “birds”) and a singular form of a word (“bird”) to the same thing (in this case, both are converted to “bird”).

Note that the output of a stemmer is not necessarily a valid word; what is important is that words with closely related meaning are converted to the same form, allowing a search to find them. For example, both the word “happy” and the word “happiness” are converted to the form “happi”, so if a document contained “happiness”, a search for “happy” would find that document.

The rules applied by a stemmer are dependent on the language of the text; Xapian includes stemmers for more than a dozen languages (and for some languages there is a choice of stemmers), built using the Snowball language. We’d like to add stemmers for more languages - see the Snowball site for information on how to contribute.


## Xapiand Stem Strategy

By default Xapiand doesn't use stem, only when `_language` is specified the `_stem_strategy` default is `stem_some`.

The stem strategies are:
* `none`

	Don't perform any stemming
* `stem_some`

	Stem all terms except for those which start with a capital letter, or are followed by certain characters (currently: \(/@<>=\*\[\{\" \), or are used with operators which need positional information. Stemmed terms are prefixed with 'Z'
* `some`

	same as `stem_some`
* `stem_all`

	Stem all terms (note: no 'Z' prefix is added)
* `all`

	same as `stem_all`
* `stem_all_z`

	Stem all terms (note: 'Z' prefix is added).
* `all_z`

	same as `stem_all_z`


## Parameters for the text fields

The following parameters are accepted by text fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_stem_language`                      | The [language](https://xapian.org/docs/apidoc/html/classXapian_1_1Stem.html#a6c46cedf2047b159a7e4c9d4468242b1) that stemming algorithm is going to use                                    |
| `_stem_strategy`                      | The [strategy](https://xapian.org/docs/apidoc/html/classXapian_1_1QueryParser.html#ac7dc3b55b6083bd3ff98fc8b2726c8fd) that stemming algorithm is going to use                                    |
| `_value`                              | The value for the field                                                                 |
| `_slot`                               | The slot number                                                                         |
| `_index`                              | One or a pair of : `none`, `field_terms`, `field_values`, `field_all`, `field`, `global_terms`, `global_values`, `global_all`, `global`, `terms`, `values`, `all`      |
| `_prefix`                             | The prefix with the term is going to be indexed     |
| `_weight`                             | The weight with the term is going to be indexed     |
