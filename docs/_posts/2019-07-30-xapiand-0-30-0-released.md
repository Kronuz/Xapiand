---
title: "Xapiand 0.30.0 Released"
date: "2019-07-30 15:48:00 -0600"
author: Kronuz
version: 0.30.0
categories: [release]
---

Adds default operator to query boolean parser and fixes threading issues during
shutdown.


### Changed
- Adds default operator for queries parsed with the boolean parser

### Fixed
- Discovery protocol callback executed by appropriate thread
- Fixes race condition during shutdown
