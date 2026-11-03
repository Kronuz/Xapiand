---
title: "Store Content API"
---

The _Store API_ allows adding additional content to documents and store such
content in the _Index Storage_.

The _Index Storage_ is designed to put files in volumes much in the way
Facebook's _Haystack_ <sup>[1](#footnote-1)</sup> works; once there a file
enters the storage it can't really be deleted/modified from the volume, but
instead, if a change is needed, a new file blob will be written to the volume.
Storage is envisioned to be used when there are files you need to store which
you know won't be changing often or at all.

Assuming there is a [PNG](/Xapiand/assets/sample.png)
file called `sample.png` in the working directory, lets add those to the storage
using `PUT` or `UPDATE` (if the document already exists) _and_ passing a content
type:

```rest
PUT /assets/sample
Content-Type: image/png

@sample.png
```

<!-- e2e:begin
---
description: Store PNG
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```
e2e:end -->

And getting it is just a matter of retreiving it using the `GET` HTTP method:

```rest
GET /assets/sample
Accept: image/png
```

<!-- e2e:begin
---
description: Get PNG (using Accept)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```
e2e:end -->

Or by visiting the link to it with your web browser:
[http://localhost:8880/assets/sample](http://localhost:8880/assets/sample)

:::hint[Toggle Console Previews]{.tip}
You can enable previews for images in the terminal using the "_very-very-very-very_"
verbose command line option (`-vvvvv`). Note you a compatible terminal for this
feature to work ([iTerm2](https://www.iterm2.com), for example).
:::

## Multi-Content Documents

Use `PUT` or `UPDATE` with a different `Content-Type` to add new content to the
same document. For example, the following adds a [PDF](/Xapiand/assets/sample.pdf)
and a [JPEG](/Xapiand/assets/sample.jpg) from
files called `sample.pdf` and `sample.jpg`, respectively:

```rest
UPDATE /assets/sample
Content-Type: application/pdf

@sample.pdf
```

<!-- e2e:begin
---
description: Store PDF (Using Content-Type)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```
e2e:end -->

This time we also include the `.jpg` selector as a [File Extension](#file-extension):

```rest
UPDATE /assets/sample.jpg
Content-Type: image/jpeg

@sample.jpg
```

<!-- e2e:begin
---
description: Store JPG (Using .jpg selector)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```
e2e:end -->

Then you can get either of them requesting the appropriate content type in
the `Accept` header:

```rest
GET /assets/sample
Accept: application/pdf
```

<!-- e2e:begin
---
description: Retrieve PDF (Using Accept)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

```js
pm.test("Response content type is PDF", function() {
    pm.response.to.be.header('Content-Type', 'application/pdf');
});
```

```js
pm.test("Response is stored PDF", function() {
    pm.expect(pm.response.stream.length).to.equal(158409);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('19d09ba48c00523219372109ef27149c1603e4ab423bce9f1b966f1ffacea66d')
});
```
e2e:end -->

### Default Content Type

:::hint[Default Content Type]{.info}
In Multi-Content Documents, the last content that was stored is the
_default content type_, if none is specified using the `Accept` header.
:::

```rest
GET /assets/sample
Accept: *
```

<!-- e2e:begin
---
description: Retrieve PDF (Using Accept)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

```js
pm.test("Response content type is JPG", function() {
    pm.response.to.be.header('Content-Type', 'image/jpeg');
});
```

```js
pm.test("Response is stored JPG", function() {
    pm.expect(pm.response.stream.length).to.equal(8879);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('98abadeba7afd803cb80e5c24b14dd1a19c50d62ac278cef1df1d6a878f4c317')
});
```
e2e:end -->

### File Extension

If passing a file extension, the default content type is obtained from the
`mime.types` file (usually in `/usr/local/share/xapiand/mime.types`).

For example, this will return the content with `image/png` content type of the
document with ID `sample`:

```rest
GET /assets/sample.png
```

<!-- e2e:begin
---
description: Retrieve PNG (Using .png selector)
---

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

```js
pm.test("Response content type is PNG", function() {
    pm.response.to.be.header('Content-Type', 'image/png');
});
```

```js
pm.test("Response is stored PNG", function() {
    pm.expect(pm.response.stream.length).to.equal(158025);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('6ec1d5285d9b3c961a90077910806bb23d957133581d73e77dde5fe3ed0f7171')
});
```
e2e:end -->

Or visiting the link to the PNG content with your web browser:
[http://localhost:8880/assets/sample.png](http://localhost:8880/assets/sample.png)

## Retrieving Information

You can get the information about the document as usual:

```rest
INFO /assets/sample
```

<!-- e2e:begin
---
description: Retrieve information
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Response with proper data", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.data.length).to.equal(4);
  pm.expect(jsonData.data[0].content_type).to.equal("application/msgpack");
  pm.expect(jsonData.data[1].content_type).to.equal("image/png");
  pm.expect(jsonData.data[2].content_type).to.equal("application/pdf");
  pm.expect(jsonData.data[3].content_type).to.equal("image/jpeg");
});
```
e2e:end -->

The result (partially shown) has the available content types listed inside
 `data`

```json
{
  "docid": 1,
  "data": [
    {
      "content_type": "application/msgpack",
      "type": "inplace"
    },
    {
      "content_type": "image/png",
      "type": "stored",
      "volume": 0,
      "offset": 512,
      "size": "462.7KiB"
    },
    {
      "content_type": "application/pdf",
      "type": "stored",
      "volume": 0,
      "offset": 60063,
      "size": "676.4KiB"
    },
    {
      "content_type": "image/jpeg",
      "type": "stored",
      "volume": 0,
      "offset": 143195,
      "size": "557.6KiB"
    }
  ], ...
}
```

## Removing Content

To remove stored content by storing an empty object:

```rest
UPDATE /assets/sample
Content-Type: image/jpeg
Content-Length: 0
```

<!-- e2e:begin
---
description: Remove content
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

:::hint{.caution}
Note removing content doesn't actually remove the blob from the volume, it
just removes the "link" to it from the document.
:::

---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)
