---
title: "Xapiand 0.12.0 Released"
date: "2019-03-10 09:09:00 -0600"
author: Kronuz
version: 0.12.0
categories: [release]
---

Fixed several issues with aggregations and bulk indexing.


### Fixed
- Aggregations with `_aggs` in different order
- JSON serialization of Infinite and NaN
- Fixed race condition during bulk indexing

### Changed
- **BREAKING**: Fields in responses renamed and nested differently:
                + `query.hits` moved to `hits`
                + `query.matches_estimated` renamed to `count`
                + `query.total_count` renamed to `doc_count`
- `MERGE` method is deprecated and will be removed, use `UPDATE` instead
- Default UUID mode is now encoded + compact
