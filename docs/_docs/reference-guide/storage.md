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
~ $ curl -XPUT -H "Content-Type: image/png" \
	'localhost:8880/twitter/images/Kronuz.png?commit' \
	--data-binary @'Kronuz.png'
```

And getting it is just a matter of retreiving it using GET:

```sh
~ $ curl -H "Accept: image/png" \
	'localhost:8880/twitter/images/Kronuz.png'
```

{: .note .unreleased}
**_TODO:_** Work in progress...


---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
