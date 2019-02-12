---
title: "Xapiand 0.8.14 Released"
date: "2019-01-27 00:00:00 -0600"
author: Kronuz
version: 0.8.14
categories: [release]
---

Added new Xapiand client for Python, based on Elasticsearch client and improves
aggregations.


### Added
- Added new Xapiand client (based on Elasticsearch client)
- Added sorting for Aggregations
- Added `_keyed` to aggregations optional. Keyed are off by default now
- Added `_shift` to aggregations

### Deprecated
- Schema deprecated `string` type, it now only has `text` and
  `keyword` (`string` will be removed at some point)
