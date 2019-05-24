---
title: "Xapiand 0.23.0 Released"
date: "2019-05-24 15:40:00 -0600"
author: Kronuz
version: 0.23.0
categories: [release]
---

This release fixes some issues in the schema when indexing nested objects and
arrays. It also renames geospatial "height" to proper "altitude" name. Added
a lot more tests and increased test coverage.


### Changed
- **BREAKING**: Geo: Renamed `height` to `altitude`
- **BREAKING**: Geo: Renamed `lng` to `lon`

### Fixed
- Schema with nested arrays and objects
