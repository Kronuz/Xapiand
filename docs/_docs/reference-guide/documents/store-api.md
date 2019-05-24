---
title: Store Content API
short_title: Store API
---

The _Store API_ allows adding additional content to documents and store such
content in the _Index Storage_.

The _Index Storage_ is designed to put files in volumes much in the way
Facebook's _Haystack_ <sup>[1](#footnote-1)</sup> works; once there a file
enters the storage it can't really be deleted/modified from the volume, but
instead, if a change is needed, a new file blob will be written to the volume.
Storage is envisioned to be used when there are files you need to store which
you know won't be changing often or at all.

Assuming there is a [PNG]({{ '/assets/Lenna.png' | absolute_url }}){:download="Lenna.png"}
file called `Lenna.png` in the working directory, lets add those to the storage
using `STORE`:

{% capture req %}

```json
STORE /assets/Lenna
Content-Type: image/png

@Lenna.png
```
{% endcapture %}
{% include curl.html req=req title="Store PNG" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

And getting it is just a matter of retreiving it using the `GET` HTTP method:

{% capture req %}

```json
GET /assets/Lenna
Accept: image/png
```
{% endcapture %}
{% include curl.html req=req title="Get PNG (using Accept)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

Or by visiting the link to it with your web browser:
[http://localhost:8880/assets/Lenna](http://localhost:8880/assets/Lenna){:target="_blank"}

{: .note .tip }
**_Toggle Console Previews_**<br>
You can enable previews for images in the terminal using the "_very-very-very-very_"
verbose command line option (`-vvvvv`). Note you a compatible terminal for this
feature to work ([iTerm2](https://www.iterm2.com){:target="_blank"}, for example).


## Multi-Content Documents

Use `STORE` with a different `Content-Type` to add new content to the same
document. For example, the following adds a [PDF]({{ '/assets/Lenna.pdf' | absolute_url }}){:download="Lenna.pdf"}
and a [JPEG]({{ '/assets/Lenna.jpg' | absolute_url }}){:download="Lenna.jpg"} from
files called `Lenna.pdf` and `Lenna.jpg`, respectively:

{% capture req %}

```json
STORE /assets/Lenna
Content-Type: application/pdf

@Lenna.pdf
```
{% endcapture %}
{% include curl.html req=req title="Store PDF (Using Content-Type)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

This time we also include the `.jpg` selector as a [File Extension](#file-extension):

{% capture req %}

```json
STORE /assets/Lenna.jpg
Content-Type: image/jpeg

@Lenna.jpg
```
{% endcapture %}
{% include curl.html req=req title="Store JPG (Using .jpg selector)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

Then you can get either of them requesting the appropriate content type in
the `Accept` header:

{% capture req %}

```json
GET /assets/Lenna
Accept: application/pdf
```
{% endcapture %}
{% include curl.html req=req title="Retrieve PDF (Using Accept)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Response content type is PDF", function() {
    pm.response.to.be.header('Content-Type', 'application/pdf');
});
```

{: .test }

```js
pm.test("Response is stored PDF", function() {
    pm.expect(pm.response.stream.length).to.equal(692615);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('66bb6df2255f34e2be54344047dad389a94be873e53a0b4c46817a3ecaeb6a61')
});
```

### Default Content Type

{: .note .info }
**_Default Content Type_**<br>
In Multi-Content Documents, the last content that was stored is the
_default content type_, if none is specified using the `Accept` header.

{% capture req %}

```json
GET /assets/Lenna
Accept: *
```
{% endcapture %}
{% include curl.html req=req title="Retrieve PDF (Using Accept)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Response content type is JPG", function() {
    pm.response.to.be.header('Content-Type', 'image/jpeg');
});
```

{: .test }

```js
pm.test("Response is stored JPG", function() {
    pm.expect(pm.response.stream.length).to.equal(570958);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('820eae76e4639a059a1bc799763ad82961ffbc8d41b58920a3f7ac622455ed46')
});
```


### File Extension

If passing a file extension, the default content type is obtained from the
`mime.types` file (usually in `/usr/local/share/xapiand/mime.types`).

For example, this will return the content with `image/png` content type of the
document with ID `Lenna`:

{% capture req %}

```json
GET /assets/Lenna.png
```
{% endcapture %}
{% include curl.html req=req title="Retrieve PNG (Using .png selector)" %}

{: .test }

```js
pm.test("Response is success", function() {
    pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Response content type is PNG", function() {
    pm.response.to.be.header('Content-Type', 'image/png');
});
```

{: .test }

```js
pm.test("Response is stored PNG", function() {
    pm.expect(pm.response.stream.length).to.equal(473831);
    // pm.expect(CryptoJS.SHA256(pm.response.stream).toString()).to.equal('7e497501a28bcf9a353ccadf6eb9216bf098ac32888fb542fb9bfe71d486761f')
});
```

Or visiting the link to the PNG content with your web browser:
[http://localhost:8880/assets/Lenna.png](http://localhost:8880/assets/Lenna.png){:target="_blank"}


## Retrieving Information

You can get the information about the document as usual:

{% capture req %}

```json
INFO /assets/Lenna
```
{% endcapture %}
{% include curl.html req=req title="Retrieve information" %}

{: .test }

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

{: .test }

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

{% capture req %}

```json
STORE /assets/Lenna
Content-Type: image/jpeg
Content-Length: 0
```
{% endcapture %}
{% include curl.html req=req title="Remove content" %}

{: .test }

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

{: .note .caution }
Note removing content doesn't actually remove the blob from the volume, it
just removes the "link" to it from the document.

---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
