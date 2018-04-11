---
title: Storage API
---

The storage is designed to put files in volumes much in the way Facebook's
Haystack <sup>[1](#footnote-1)</sup> works; once there a file enters the
storage it can't really get deleted/modified from the volume, but instead, if
a change is needed, a new file blob will be written to the volume. Storage is
envisioned to be used when there are files you need to store which you know
won't be changing often.

Lets add something to the storage using `STORE`:

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

{: .note}
**_Toggle console previews_**<br>
You can enable previews for images in the terminal using the very-very-very-very
verbose command line option (`-vvvvv`). Note you a compatible terminal for this
feature to work ([iTerm2](https://www.iterm2.com){:target="_blank"}, for example).

## Multi-Content Documents

Use `STORE` with a different Content-Type to add new content to the same document:

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

## Removing Content

Tou remove stored content by storing an empty object:

{% capture req %}

```json
STORE /twitter/images/Kronuz
Content-Type: image/jpeg
Content-Length: 0
```
{% endcapture %}
{% include curl.html req=req %}

---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
