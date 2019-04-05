---
title: Welcome
redirect_from: /docs/
---

This documentation aims to be a comprehensive guide to {{ site.name }}. We'll cover
topics such as getting your site up and running, indexing and searching
documents, customizing data schemas, deploying to various environments, and
give you some advice on participating in the future development of {{ site.name }}
itself.


## So what is {{ site.name }}, exactly?

Xapiand is **A Modern Highly Available Distributed RESTful Search and Storage
Server built for the Cloud**. It takes JSON documents and indexes them
efficiently for later retrieval.


---

## Document Conventions

Throughout this guide there are a few conventions in the way documentation
is laid out and formatted, these are a few of them.


### API Snippets

Many examples of API calls that look something like this:

{% capture req %}

```json
PUT /twitter/user/Kronuz

{
  "name" : "German M. Bravo"
}
```
{% endcapture %}
{% include curl.html req=req %}

For those, you can always copy the equivalent _curl_ code for it by hovering the
snippet and clicking on the copy to the clipboard button:
&nbsp;&nbsp;<i class="fa fa-clipboard" style="color: {{ site.theme_color_dark }}"></i>


### Helpful Hints

A number of small-but-handy pieces of information that can make using
{{ site.name }} easier, more interesting, and less hazardous. We use several
different strategies to draw your attention to certain pieces of information.
Here's what to look out for:

{: .note .tip }
**_Tips help you get more from {{ site.name }}_**<br>
These are tips and tricks that will help you be a {{ site.name }} wizard!

{: .note .info }
**_Notes are handy pieces of information_**<br>
These are for the extra tidbits sometimes necessary to understand {{ site.name }}.

{: .note .caution }
**_Things to notice to help you get better results_**<br>
Be aware of these messages if you wish to avoid certain death.

{: .note .warning }
**_Warnings help you not blow things up_**<br>
Be aware of these messages if you wish to avoid certain death.

{: .note .unimplemented }
**_You'll see this by a feature that hasn't been released_**<br>
Some pieces are for future versions of {{ site.name }} that are not yet released.

{: .note .construction }
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

If you come across anything along the way that we haven't covered, or if you
know of a tip you think others would find handy, please [file an
issue]({{ site.repository }}/issues/new) and we'll see about
including it in this guide.


---

Development of {{ site.name }} is done as an open-source project and maintaining it is
hard and time consuming, if you like {{ site.name }}, please donate whatever
you feel comfortable. We accept donations via PayPal:

<a class="paypalme" href="https://www.paypal.me/Kronuz/25" target="_blank" rel="nofollow">Donate to Kronuz via PayPal</a>
