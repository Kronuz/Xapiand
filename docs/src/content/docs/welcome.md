---
title: "Welcome"
---

This documentation aims to be a comprehensive guide to Xapiand. We'll cover
topics such as getting your server up and running, indexing and searching
documents, customizing data schemas, deploying to various environments, and
give you some advice on participating in the future development of Xapiand
itself.

## So what is Xapiand, exactly?

Xapiand is **A Modern Highly Available Distributed RESTful Search and Storage
Server built for the Cloud**. It takes JSON documents and indexes them
efficiently for later retrieval.

---

## Document Conventions

Throughout this guide there are a few conventions in the way documentation
is laid out and formatted, these are a few of them.

### API Snippets

Many examples of API calls that look something like this:

```rest
PUT /twitter/user/Kronuz

{
  "name": "Germán Méndez Bravo"
}
```

For those, you can always copy the equivalent _curl_ code for it by hovering the
snippet and clicking on the copy to the clipboard button:
&nbsp;&nbsp;<i class="fa fa-clipboard" style="color: "></i>

### Helpful Hints

A number of small-but-handy pieces of information that can make using
Xapiand easier, more interesting, and less hazardous. We use several
different strategies to draw your attention to certain pieces of information.
Here's what to look out for:

:::hint[Tips help you get more from Xapiand]{.tip}
These are tips and tricks that will help you be a Xapiand wizard!
:::

:::hint[Notes are handy pieces of information]{.info}
These are for the extra tidbits sometimes necessary to understand Xapiand.
:::

:::hint[Things to notice to help you get better results]{.caution}
Be aware of these messages if you wish to avoid certain death.
:::

:::hint[Warnings help you not blow things up]{.warning}
Be aware of these messages if you wish to avoid certain death.
:::

:::hint[You'll see this by a feature that hasn't been released]{.unimplemented}
Some pieces are for future versions of Xapiand that are not yet released.
:::

:::hint[Documentation section under construction]{.construction}
This part of the Xapiand documentation is still not completed.
:::

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
issue](https://github.com/Kronuz/Xapiand/issues/new) and we'll see about
including it in this guide.

---

Development of Xapiand is done as an open-source project and maintaining it is
hard and time consuming, if you like Xapiand, please donate whatever
you feel comfortable. We accept donations via PayPal:

<a class="paypalme" href="https://www.paypal.me/Kronuz/25" target="_blank" rel="nofollow">Donate to Kronuz via PayPal</a>
