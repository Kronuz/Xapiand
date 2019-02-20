---
title: "Xapiand 0.10.0 Released"
date: "2019-02-20 11:10:00 -0600"
author: Kronuz
version: 0.10.0
categories: [release]
---

This release fixes a few issues while searching geospatial data and changes
default accuracies for numbers and geospatial levels. It also adds
`_match_all` and `_match_none` and changes the default stopper during indexation
(to "stop stemmed").


### Added
- Added `_match_all` and `_match_none` to `:search` command
- Added the Elite Set Operator

### Changed
- Use OP_FILTER for terms in range queries
- Flattened range queries
- The default stopper during indexation is now `stop_stemmed`
- Numeric ranges are now properly calculating the filtering terms
- Geospatial queries are now calculating better the filtering trixels terms
- Default accuracies changed
- Removed `_raw` from QueryDSL

### Fixed
- Fixed `sort` query param during search
