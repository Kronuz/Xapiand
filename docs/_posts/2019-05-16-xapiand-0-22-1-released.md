---
title: "Xapiand 0.22.1 Released"
date: "2019-05-16 10:49:00 -0600"
author: Kronuz
version: 0.22.1
categories: [release]
---

This release significantly boosts performance of RESTORE method by launching
parallel document indexers. Queries now accept wildcard globs; this allows
using `?` and `*` anywhere in the query. Also fixes RESTORE for binaries
compiled with GCC.


### Changed
- Performance improvements with restore by launching parallel DocIndexers
- Queries using glob wildcards

### Fixed
- Nested arrays and objects in arrays
- Restoring for GCC compiled Xapiand
