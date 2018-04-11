---
title: Storage API
---

The storage is designed to put files in volumes much in the way Facebook's
Haystack <sup>[1](#footnote-1)</sup> works; once there a file enters the
storage it can't really get deleted/modified from the volume, but instead, if
a change is needed, a new file blob will be written to the volume. Storage is
envisioned to be used when there are files you need to store which you know
won't be changing often.

Lets put something in the storage using PUT:

```sh
~ $ curl -H 'Content-Type: image/png' \
  --data-binary '@Kronuz.png' \
  -X PUT 'localhost:8880/twitter/images/Kronuz'
```

And getting it is just a matter of retreiving it using GET:

```sh
~ $ curl -H 'Accept: image/png' \
  'localhost:8880/twitter/images/Kronuz'
```

{: .note}
**_Toggle console previews_**<br>
You can enable previews for images in the terminal using the very-very-very
verbose command line option (`-vvvvv`). Note you a compatible terminal for this
feature to work ([iTerm2](https://www.iterm2.com){:target="_blank"}, for example).

## Multi-Content Documents

Use `MERGE` instead of `PUT` to add new content types to the same document:

```sh
~ $ curl -H 'Content-Type: image/jpeg' \
  --data-binary '@Kronuz.jpg' \
  -X MERGE 'localhost:8880/twitter/images/Kronuz'
```

Then you can get either of them requesting the appropriate Content-Type:

```sh
~ $ curl -H 'Accept: image/jpeg' \
  'localhost:8880/twitter/images/Kronuz'
```

{: .note .unreleased}
**_TODO:_** Work in progress...


---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
