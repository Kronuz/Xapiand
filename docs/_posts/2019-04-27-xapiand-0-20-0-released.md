---
title: "Xapiand 0.20.0 Released"
date: "2019-04-27 15:33:00 -0600"
author: Kronuz
version: 0.20.0
categories: [release]
---

This release fixes quite a few long-standing issues; including full remote
protocol support for all queries and remote schemas cache invalidation.

A major bug in the xapian core remote database protocol was detected and fixed
(https://trac.xapian.org/ticket/783) which produced weird and unexpected errors
at times.


### Fixed
- **BREAKING**: Remote Protocol issue which provoked messaged to get out of sync
- Issues schemas not being saved (unexpected "_id" errors)
- Detection of remote schema updates
- Certain cluster remote queries
