---
title: Storage API
---

The storage is designed to put files in volumes much in the way Facebook's
Haystack <sup>[1](#footnote-1)</sup> works; once there a file enters the
storage it can't really be deleted/modified from the volume, but instead, if
a change is needed, a new file blob will be written to the volume. Storage is
envisioned to be used when there are files you need to store which you know
won't be changing often or at all.

Assuming there's a PNG image file called `Kronuz.png` in the working directory,
lets add it to the storage using `STORE`:

{% capture req %}
```json
STORE /twitter/images/Kronuz
Content-Type: image/png

@Kronuz.png
```
{% endcapture %}
{% include curl.html req=req %}

And getting it is just a matter of retreiving it using GET:

{% capture req %}
```json
GET /twitter/images/Kronuz
Accept: image/png
```
{% endcapture %}
{% include curl.html req=req %}

Or by visiting the link to it with your web browser:
[http://localhost:8880/twitter/images/Kronuz](http://localhost:8880/twitter/images/Kronuz){:target="_blank"}

{: .note}
**_Toggle console previews_**<br>
You can enable previews for images in the terminal using the "_very-very-very-very_"
verbose command line option (`-vvvvv`). Note you a compatible terminal for this
feature to work ([iTerm2](https://www.iterm2.com){:target="_blank"}, for example).

## Multi-Content Documents

Use `STORE` with a different Content-Type to add new content to the same document:

{% capture req %}
```json
STORE /twitter/images/Kronuz
Content-Type: application/pdf

@Kronuz.pdf
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}
```json
STORE /twitter/images/Kronuz
Content-Type: image/jpeg

@Kronuz.jpg
```
{% endcapture %}
{% include curl.html req=req %}

Then you can get either of them requesting the appropriate Content-Type:

{% capture req %}
```json
GET /twitter/images/Kronuz
Accept: image/jpeg
```
{% endcapture %}
{% include curl.html req=req %}

## Retrieving Information

You can get the information about the document as usual:

{% capture req %}
```json
GET /twitter/images/:info/Kronuz?pretty
```
{% endcapture %}
{% include curl.html req=req %}

The result (partially shown) has the available content types listed inside
 `#document_info ➛ #blobs`

```json
{
  "#document_info": {
    "#blobs": {
      "image/png": {
        "#type": "stored",
        "#volume": 0,
        "#offset": 512,
        "#size": 572272
      },
      "image/jpeg": {
        "#type": "stored",
        "#volume": 0,
        "#offset": 72411,
        "#size": 484591
      }
    }, ...
  }
}
```

## Removing Content

To remove stored content by storing an empty object:

{% capture req %}
```json
STORE /twitter/images/Kronuz
Content-Type: image/jpeg
Content-Length: 0
```
{% endcapture %}
{% include curl.html req=req %}

Note removing content doesn't actually remove the blob from the volume, it
just removes the "link" to it from the document.

---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
