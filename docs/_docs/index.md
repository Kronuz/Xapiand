---
title: Welcome
permalink: /docs/welcome/
redirect_from: /docs/
---

This documentation aims to be a comprehensive guide to Xapiand. We'll cover
topics such as getting your site up and running, indexing and searching
documents, customizing data schemas, deploying to various environments, and
give you some advice on participating in the future development of Xapiand
itself.


## So what is Xapiand, exactly?

Xapiand is *A Modern Highly Available Distributed RESTful Search and Storage
Server built for the Cloud*. It takes JSON (or MessagePack) documents and
indexes them efficiently for later retrieval.

---

## Helpful Hints

Throughout this guide there are a number of small-but-handy pieces of
information that can make using {{ site.name }} easier, more interesting, and
less hazardous. Here's what to look out for.

{: .note}
**_Tips help you get more from {{ site.name }}_**<br>
These are tips and tricks that will help you be a {{ site.name }} wizard!

{: .note .info}
**_Notes are handy pieces of information_**<br>
These are for the extra tidbits sometimes necessary to understand {{ site.name }}.

{: .note .warning}
**_Warnings help you not blow things up_**<br>
Be aware of these messages if you wish to avoid certain death.

{: .note .unreleased}
**_You'll see this by a feature that hasn't been released_**<br>
Some pieces are for future versions of {{ site.name }} that are not yet released.

{: .note .construction}
**_Documentation section under construction_**<br>
This part of the {{ site.name }} documentation is still not completed.

<!--
## Keyboard symbols used:
- Ctrl: ⌃
- Alt: ⎇
- Cmd: ⌘
- Windows: ❖
- Backspace: ⌫
- Enter: ⏎
- Shift: ⇫
- Caps lock: ⇪
- Arrows: ⇦⇧⇨⇩
- Others: ➛
-->

---

## API Snippets

Throughout this guide there are many examples of API calls that look something
like this:

{% capture req %}
```json
PUT /twitter/user/Kronuz

{
  "name" : "German M. Bravo"
}
```
{% endcapture %}
{% include curl.html req=req %}

For those, you can always copy the equivalent curl code for it by hovering the
snippet and clicking on the copy to the clipboard button: &nbsp;&nbsp;<i class="fa fa-clipboard"></i>

---

## Donate to {{ site.name }}!

### How will my donation be used?

Development of *{{ site.name }}* is done as an open-source project and
maintaining it is hard and time consuming, if you like {{ site.name }}, please
donate whatever you feel comfortable with via PayPal. The donations will be
directly used to support {{ site.name }}'s developers.

### How do I donate?

We accept donations via PayPal:

<a class="paypalme" href="https://www.paypal.me/Kronuz/25" target="_blank" rel="nofollow">Donate to Kronuz via PayPal</a>


---

If you come across anything along the way that we haven't covered, or if you
know of a tip you think others would find handy, please [file an
issue]({{ site.repository }}/issues/new) and we'll see about
including it in this guide.
