---
title: "Xapiand 0.6.1 Released"
date: "2018-04-25 18:00:00 -0600"
author: Kronuz
version: 0.6.1
categories: [release]
---

A lot of optimizations. Xapiand is now using a whole new threadpool with
lock-free queue for improved performance.

*BREAKING CHANGE* A problem with generated terms in geospatial fields will
require reindexing documents using such geospatial fields.

Restore command is now indexing documents in parallel using multiple threads
which yields better performance while bulk indexing.

The release also fixes a serious multithreading bug with collected statistics.

- Parallel restore commands
- General optimizations
- Fixed geospatial terms and accuracy
- Fixes storage (no longer returns 500)
- Fixes multithreading bug with collected statistics
