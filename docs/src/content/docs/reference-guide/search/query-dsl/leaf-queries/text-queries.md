---
title: "Text Queries"
---

The high-level full text queries are usually used for running full text queries
on full text fields like the body of an email. They understand how the field
being queried is process which means converting text, like the body of any
email, into tokens or terms which are added to the inverted index for searching.

* [**_Simple Query_**](simple)
* [**_Phrase Query_**](phrase)
* [**_Wildcard Query_**](wildcard)
* [**_Partial Query_**](partial)
* [**_Fuzzy Query_**](fuzzy)
* [**_Near Query_**](near)
* [**_Love and Hate Query_**](love-and-hate)
* [**_Default Operator_**](default-operator)
* [**_Stop Words_**](stop-words)

## The query string grammar

The text of a text query (`"personality": "..."`, or the value after a field in `q=`)
is parsed by the [queryparser](https://github.com/Kronuz/queryparser) library, with the words
analysed by the field's own rules (language, stemming, stop words) so that query terms equal
index terms. The syntax, with the flag that enables each part:

| Syntax | Meaning | Applies |
|---|---|---|
| `a b` | The default operator joins the words: `OR`. | always |
| `"a b"`, `a-b`, `a:b`, `a/b` | The words as a [phrase](text-queries/phrase). | always |
| `+a`, `-a` | `a` is required, or excluded ([Love and Hate](text-queries/love-and-hate)). | always |
| `a*` | Every word that starts with `a` ([wildcard](text-queries/wildcard)). | always |
| `b?a`, `ba*c*` | Glob patterns inside a word. | with `_wildcard` |
| `ba**`, the last word | Words that start with it, at most 50 ([partial](text-queries/partial)). | `**`, or with `_partial` |
| `word~`, `word~n` | Words within edit distance `n` (2 by default) of the last word ([fuzzy](text-queries/fuzzy)). | always, at the end of the text |
| `a AND b`, `a OR b`, `a NOT b`, `a XOR b`, `a AND NOT b` | Boolean operators: OR binds loosest, then XOR, then AND and NOT. | only with [`_and` or `_or`](text-queries/default-operator) |
| `a NEAR b`, `a ADJ b`, `NEAR/n`, `ADJ/n` | The words within a window of `n` + words - 1 positions (`n` is 10 by default; [near](text-queries/near)). | only with `_and` or `_or` |
| `( )` | Groups. | always |

The words of a group that are stop words are dropped unless the group holds only stop
words (see [Stop Words](text-queries/stop-words)). A plain string never reads `AND`, `OR`,
`NOT` or `XOR` as operators: they are words. The `_and` and `_or` modifiers choose the
default operator and turn the boolean words on:

```rest
SEARCH /bank/?limit=0

{
  "_query": {
    "personality": "ambitious adventurous"
  }
}
```

<!-- e2e:begin
---
description: Grammar: the default operator is OR
---

```js
pm.test("Two words match either", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(89);
});
```
e2e:end -->

```rest
SEARCH /bank/?limit=0

{
  "_query": {
    "personality": {
      "_and": "ambitious adventurous"
    }
  }
}
```

<!-- e2e:begin
---
description: Grammar: _and joins the words with AND
---

```js
pm.test("_and needs both words", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(2);
});
```
e2e:end -->

```rest
SEARCH /bank/?limit=0

{
  "_query": {
    "personality": {
      "_and": "ambitious NOT adventurous"
    }
  }
}
```

<!-- e2e:begin
---
description: Grammar: boolean words under _and
---

```js
pm.test("NOT excludes the second word", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(39);
});
```
e2e:end -->

```rest
SEARCH /bank/?limit=0

{
  "_query": {
    "personality": "+ambitious -adventurous"
  }
}
```

<!-- e2e:begin
---
description: Grammar: required and excluded words
---

```js
pm.test("Required and excluded words", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(39);
});
```
e2e:end -->

```rest
SEARCH /bank/?limit=0

{
  "_query": {
    "personality": "ambitious AND adventurous"
  }
}
```

<!-- e2e:begin
---
description: Grammar: AND in a plain string is a word
---

```js
pm.test("A plain string does not read AND as an operator", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(89);
});
```
e2e:end -->

A text that does not parse, such as an operator with a missing operand under `_and`, answers
`400`. Sources: the grammar table in the queryparser's README; the flags the node sets,
`src/query_dsl.cc:805-848`; `_and` and `_or` set the boolean flag, `src/query_dsl.cc:817-822`.
