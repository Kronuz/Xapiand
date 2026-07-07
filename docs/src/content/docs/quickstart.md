---
title: "Quick-start"
---

This guide will take you through the process of installing Xapiand and
familiarize you with the concepts that will allow you to use search and
storage indexes. **DON'T PANIC**, it will take just a few minutes.

---

## Installing and Running

### Using Docker

```sh
# Run from Docker with *very-very-very* verbose output:
docker run -p 8880:8880 --rm ghcr.io/kronuz/xapiand:latest -vvvv
```

### Using Homebrew under macOS

```sh
# Install with Homebrew:
~ $ brew install Kronuz/tap/xapiand

# Run in foreground with *very-very-very* verbose output:
~ $ xapiand -vvvv
```

You can also check the [Installation](/Xapiand/installation)
section for more details.

:::hint[High Verbosity]{.tip}
For testing, we recommend running with _*very-very-very* verbose_ output, which
can be specified by using `-vvvv` or `--verbosity=4`. This will log all full
requests and will also turn on `pretty`, `human`, `echo` and `comments` options.
See [Options](/Xapiand/options) for more details.
:::

---

## Indexing

Let's try and index some twitter like information. First, let's create a
twitter user, and add some tweets (the twitter index will be created
automatically):

```rest
PUT /twitter/user/Kronuz

{
  "name": "Germán Méndez Bravo"
}
```

```rest
PUT /twitter/tweet/1

{
  "user": "Kronuz",
  "postDate": "2016-11-15T13:12:00",
  "message": "Trying out Xapiand, so far, so good... so what!"
}
```

```rest
PUT /twitter/tweet/2

{
  "user": "Kronuz",
  "postDate": "2016-10-15T10:31:18",
  "message": "Another tweet, will it be indexed?"
}
```

You can dig a little deeper in the [Data Manipulation](/Xapiand/manipulation) section.

---

## Searching

Now, let's retrieve the information we just added by GETting it:

```rest
GET /twitter/user/Kronuz
```

<!-- e2e:begin
```js
pm.test("retrieved the indexed user", function () {
  pm.expect(pm.response.json().name).to.eql("Germán Méndez Bravo");
});
```
e2e:end -->

```rest
GET /twitter/tweet/1
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching document", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData["user"]).to.equal("Kronuz");
  pm.expect(jsonData["postDate"]).to.equal("2016-11-15T13:12:00");
  pm.expect(jsonData["message"]).to.equal("Trying out Xapiand, so far, so good... so what!");
});
```
e2e:end -->

```rest
GET /twitter/tweet/2
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching document", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData["user"]).to.equal("Kronuz");
  pm.expect(jsonData["postDate"]).to.equal("2016-10-15T10:31:18");
  pm.expect(jsonData["message"]).to.equal("Another tweet, will it be indexed?");
});
```
e2e:end -->

Let's find all the tweets that Kronuz posted:

```rest
SEARCH /twitter/tweet/?q=user:Kronuz
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(2);
  pm.expect(jsonData.hits.length).to.equal(2);
});
```
e2e:end -->

You can find out more in the [Data Exploration](/Xapiand/exploration)
section.
