---
title: "Copyright Notice"
---

## Xapiand
Copyright &copy; 2026 Germán Méndez Bravo (Kronuz)<br />
Copyright &copy; 2015-2020 Dubalu LLC

Xapiand is licensed under **the MIT License** (see `LICENSE`). The search engine is
the _strata_ library, also MIT licensed, and the other libraries Xapiand is built
from are fetched from [github.com/Kronuz](https://github.com/Kronuz).

The third-party components bundled in the binary and the packages, with their
licenses and copyright notices, are listed in `THIRD-PARTY-NOTICES`. Every
package installs `LICENSE`, `NOTICE` and `THIRD-PARTY-NOTICES` into
`share/doc/xapiand`.

### Modified and configured third-party code

_Lua_ 5.4.7 (MIT, Copyright &copy; 1994-2024 Lua.org, PUC-Rio) is built with
`contrib/lua/xapiand-limits.patch`, which adds resource-accounting calls to its
sources; the patch is MIT like the rest of Xapiand. _RapidJSON_ (MIT, Copyright
&copy; 2015 THL A29 Limited, a Tencent company, and Milo Yip) is used
unmodified; the build only sets its default parse flags. Their license texts are
in `THIRD-PARTY-NOTICES`.

### Google Maps

`site/public/assets/el_cerrito.png` is a Google Maps screenshot ("Map data
&copy;2019 Google", under Google's terms) with Xapiand's HTM trixels drawn over
it. The trixel overlay is Xapiand's; the image as a whole is not MIT.

### Elasticsearch

The Python client in `contrib/python/xapiand-py` is a fork of _elasticsearch-py_,
Copyright &copy; 2017 Elasticsearch, licensed under the Apache License,
Version 2.0 (see `contrib/python/xapiand-py/LICENSE`). It is not part of any
package.

This documentation is heavily based upon the _Elasticsearch_ project's, whose
text and code samples were licensed under the Apache License, Version 2.0.
