---
title: Values Aggregation
---

A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique value.

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).


{: .note .construction}
**_TODO:_** This section is a work in progress...

<div style="min-height: 800px"></div>
