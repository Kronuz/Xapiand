---
title: "Xapiand 0.29.0 Released"
date: "2019-07-25 14:51:00 -0600"
author: Kronuz
version: 0.29.0
categories: [release]
---

Fixes foreign schemas, global queries searching for strings and default operator.


### Changed
- **BREAKING**: Foreign schemas no longer accept selectors
- **BREAKING**: Global string fields indexed as `text` datatype
- QueryDSL using `_and` or `_or` also enable booleans

### Fixed
- Fix foreign schemas URIs
- Fixes issue #19
- Fixes issue #20
