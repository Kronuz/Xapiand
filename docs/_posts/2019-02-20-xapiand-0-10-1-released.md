---
title: "Xapiand 0.10.1 Released"
date: "2019-02-20 12:00:00 -0600"
author: Kronuz
version: 0.10.1
categories: [release]
---

This release fixes a bug that prevented xapiand start without specifying a
working directory. Default working directory is now `/var/run/xapiand`
(or `/usr/local/var/run/xapiand`).


### Fixed
- Default database path is /var/run/xapiand
