---
title: "Xapiand 0.19.5 Released"
date: "2019-04-13 12:30:00 -0600"
author: Kronuz
version: 0.19.5
categories: [release]
---

Fixes throttling of scheduled operations (commits, updates, syncs, etc.)
It previously had a bug that caused throttling not to work; disabling
throttling for commits triggered the bug which resulted in an exception
being thrown at certain occasions.


### Fixed
- Throttling of commits (exception was being thrown)
