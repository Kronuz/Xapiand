---
title: "Quick-start"
---

This guide takes you through installing Xapiand, starting a node, and using it
to index and search documents. It takes a few minutes.

---

## Installing and Running

### Using Docker

```sh
# Run from Docker with *very-very-very* verbose output:
docker run -p 8880:8880 --rm ghcr.io/kronuz/xapiand:latest -vvvv
```

The container keeps its data in `/var/db/xapiand`. Mount a volume there
(`-v xapiand-data:/var/db/xapiand`) to keep the indexes after the container
stops.

### Using Homebrew under macOS

```sh
# Install with Homebrew:
brew install Kronuz/tap/xapiand

# Run in foreground with *very-very-very* verbose output:
xapiand -vvvv
```

You can also check the [Installation](/Xapiand/installation)
section for more details.

:::hint[High Verbosity]{.tip}
For testing, we recommend running with _*very-very-very* verbose_ output, which
can be specified by using `-vvvv` or `--verbosity=4`. This will log all full
requests and will also turn on `pretty`, `human`, `echo` and `comments` options.
See [Options](/Xapiand/options) for more details.
:::

### The node

A single node serves the whole REST API over HTTP on port 8880 (`--port` sets
another). It keeps every index under its data directory (`-D`, `--database`;
`<prefix>/var/db/xapiand` unless given), and it logs `Node <name> is Ready to
Rock!` when it is ready to serve. Stop it with `Ctrl-C` (`SIGINT`) or `SIGTERM`:
it finishes the requests in flight, commits and closes its indexes, and exits
(see [Stopping the node](/Xapiand/options#stopping-the-node)).

Ask the node about itself:

```rest
GET /
```

<!-- e2e:begin
---
description: Ask the node about itself
---

```js
pm.test("The node is ready", function () {
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.be.a('string');
  pm.expect(jsonData.server.name).to.equal("Xapiand");
  pm.expect(jsonData.state).to.equal("READY");
});
```
e2e:end -->

The answer carries the node's `name`, the `server` and `versions` it runs, the
`options` it was started with, and its `state`.

---

## Indexing

Let's index some twitter like information. In `PUT /twitter/user/Kronuz`, the
path `twitter/user` names the index and `Kronuz` is the document ID. Writing to
an index that does not exist creates it, so we create a twitter user and add
some tweets, each kind in its own index (`twitter/user` and `twitter/tweet`):

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

### What a write answers

A node started without `-vvvv` does not echo what it stored: a `PUT` with an ID
answers `204 No Content` with an empty body.

```rest
PUT /twitter/user/Quiet

{
  "name": "Quiet Coder"
}
```

<!-- e2e:begin
---
description: PUT answers 204 without echo
params: echo=false
---

```js
pm.test("An ID in the URL answers 204 with no body", function () {
  pm.response.to.have.status(204);
  pm.expect(pm.response.text()).to.equal("");
});
```
e2e:end -->

A `POST` to the index, which has no ID, answers `201 Created` with the new
document's path in the `Location` header:

```rest
POST /twitter/user/

{
  "name": "Anonymous Coder"
}
```

<!-- e2e:begin
---
description: POST answers 201 and a Location without echo
params: echo=false
---

```js
pm.test("No ID in the URL answers 201 with a Location", function () {
  pm.response.to.have.status(201);
  pm.expect(pm.response.headers.get("Location")).to.match(/^\/twitter\/user\/.+/);
  pm.expect(pm.response.text()).to.equal("");
});
```
e2e:end -->

With `echo` on, a write answers `200 OK` with the document, its `_id` and its
`_version`. Turn it on for a node (`--echo`, or `-vvvv`) or per request
(`?echo`); `?echo=false` turns it off for one request. The examples in this
documentation run with `echo` on, so the responses they show carry the
document.

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

A write is visible to the next read once the node has answered it, so a search
right after the `PUT` finds the document. Let's find all the tweets that Kronuz
posted:

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

The same search with `COUNT` returns only the number of matches:

```rest
COUNT /twitter/tweet/?q=user:Kronuz
```

<!-- e2e:begin
---
description: COUNT the tweets of Kronuz
---

```js
pm.test("COUNT answers the total", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(2);
});
```
e2e:end -->

You can find out more in the [Data Exploration](/Xapiand/exploration)
section.
