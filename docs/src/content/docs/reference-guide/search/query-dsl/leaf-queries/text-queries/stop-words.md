---
title: "Stop Words"
---

Xapiand supports a stop word list, which allows you to specify what words
should be removed from a query before processing. This list can be overridden
or stop words can still be searched for if desired, but by default any words
in the active stop words list will not be searched for. A field has a stop word list
only when it has a `_language` (see [text fields](/Xapiand/reference-guide/schemas/field-types/text-type)):
with no language there is no list and no word is dropped. A language that has no list
is an error, never a field that quietly stops nothing.

## Where the Lists Come From

The lists of the 18 shipped languages (`ca`, `da`, `de`, `en`, `es`, `eu`, `fi`, `fr`, `hu`, `hy`, `it`, `nl`,
`no`, `pt`, `ro`, `ru`, `sv` and `tr`, 7,342 words in all) are compiled into the `xapiand` binary: a node reads
no file for them and a lookup of a shipped language never fails.

A file `<data path>/stopwords/<language>.txt` replaces the list of that language. It holds words separated by
white space, in any order and with repeats; the node reads every such file once, at startup, and a file that
cannot be read stops the node instead of leaving the shipped list in its place. A file with a name that is not
a shipped language defines a list of that name. The install copies the shipped lists to
`share/doc/xapiand/stopwords/`, a directory nothing reads: copy one into `<data path>/stopwords/` and edit it to
override that language.

Each list has a fingerprint, 16 hexadecimal digits of the hash of its sorted words, so the same words give the
same fingerprint whether they are compiled in or read from a file. An index records the fingerprint of every
list it indexed with. When that list is missing later, or its words are not the recorded ones (a file replaced
it, or a newer release ships another list), the index is refused with `409 Conflict` and a message that names the
index, the list and both fingerprints, because a query that stopped other words than the indexing did would answer
differently without saying so. A search over several indexes checks each of them, so one refused index refuses the
search. Restore the list file the index was built with (`<data path>/stopwords/<language>.txt`) and restart the node to
open the index again: an index cannot be deleted or re-created over HTTP, and a dump carries no schema.

A file is read as text with one or more words per line; a UTF-8 byte order mark is ignored. A directory named like a
list stops the node, as an unreadable file does.

The fingerprint is stored in the field's properties, as `_stop_fingerprint` beside `_language`, and is not shown
in the schema a request reads: it is derived from the list, not a setting, and a schema sent back with a stale one
would be refused. An index created before the fingerprints has none recorded: it takes the current list's
fingerprint the first time it is opened for writing, and the node logs one line naming the index and the list.

### Example

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "these days are few and far between"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Stop words are filtered from the query", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.total).to.equal(292);
});
```
e2e:end -->

The stop words in the phrase (`these`, `are`, `few`, `and`, `far`, `between`: all
are in the English list) are removed before the query runs, and only the remaining
content word, `days`, is searched. That is why documents match even though none of them
contain the whole phrase. The words of a group that are stop words are dropped
unless the group holds only stop words.

## Searching of Stop Words

Stop words can be searched by using [Love and Hate Modifiers](/Xapiand/reference-guide/search/query-dsl/leaf-queries/text-queries/love-and-hate)
(by adding `+` to the desired stop word):

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "+these days +are +few +and +far +between"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching of Stop Words results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
  pm.expect(jsonData.hits.length).to.equal(0);
});
```
e2e:end -->

An empty set of stop words under a `_stopwords` key, which would do the same, is not
implemented: `_stopwords` is not a keyword and the request answers `400`
("Invalid operator").

:::hint[Unimplemented Feature!]{.unimplemented}
This feature hasn't yet been implemented...
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_value": "these days are few and far between",
      "_stopwords": []
    }
  }
}
```

<!-- e2e:begin
---
status: 400
---
e2e:end -->
