---
title: "Open / Close Index APIs"
---

:::hint[Unimplemented Feature!]{.unimplemented}
This feature hasn't yet been implemented...
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

The _Open / Close Index APIs_ allow to close an index, and later on opening it.
A closed index has almost _no overhead_ on the cluster, and is blocked for
read/write operations. A closed index can be opened which will then go through
the normal recovery process.

The REST endpoint command are `CLOSE` and `OPEN`. For example:

```rest
CLOSE /my_index/
```

<!-- e2e:begin
---
status: 501
---
e2e:end -->

```rest
OPEN /my_index/
```

<!-- e2e:begin
---
status: 501
---
e2e:end -->
