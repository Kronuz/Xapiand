---
title: "Xapiand 0.9.0 Released"
date: "2019-02-11 17:30:00 -0600"
author: Kronuz
version: 0.9.0
categories: [release]
---

This is a rather big release, with lots and lots of improvements and bug fixes.
Mainly this one significantly increases performance, fixes issues with Geospatial
queries, embeds Xapian core 1.5.0, and adds lots of documentation and examples.


### Added
- Embedded Xapian core 1.5.0 in the source code
- Accept application/x-ndjson as input for `:restore` command
- Home and docs site with cosmetic improvements

### Changed
- Greatly improves performance
- Documentation extended
- Upgraded to fmtlib v5.3.1 library
- Upgraded to ChaiScript v6.1.1 library
- Sort accepting casts
- QueryDSL with flattened queries

### Fixed
- Fixes a few issues with the way geospatial locations were
  being filtered and indexed
