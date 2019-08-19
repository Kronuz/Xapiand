---
title: "Xapiand 0.33.0 Released"
date: "2019-08-19 10:16:00 -0600"
author: Kronuz
version: 0.33.0
categories: [release]
---

Fixes scripts in strict mode


### Changed
- Send errors in script as 400
- Notify of database changes during periodic cleanups
- Limit shutdown retries to exit
- Prevent queries ending with <space><wildcard> to match all

### Fixed
- Fix scripts in strict mode
