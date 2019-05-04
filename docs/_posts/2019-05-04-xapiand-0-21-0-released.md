---
title: "Xapiand 0.21.0 Released"
date: "2019-05-04 18:27:00 -0600"
author: Kronuz
version: 0.21.0
categories: [release]
---

This release features parallelized queries as well as fixes a serious issue
where sockets were being leaked.


### Changed
- Default do check_at_least one when using aggregations
- Updated xapian-core, fixes issues with leaking sockets
- **BREAKING**: Added patches for xapian-core to handle parallelized queries

### Fixed
- Fix issues when starting servers simultaneously
