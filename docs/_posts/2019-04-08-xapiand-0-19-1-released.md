---
title: "Xapiand 0.19.1 Released"
date: "2019-04-08 17:40:00 -0600"
author: Kronuz
version: 0.19.1
categories: [release]
---

Added fuzzy search and cjk support. Fixes a problem with searching ranges as
IDs and schema now properly saves formatted dates.


## Added
- Added CJK NGRAM and CJK words as parameters to 'text' datatype
- Enable fuzzy searches in queries: use '~'. E.g. "unserten~3" would expand to "uncertain"

## Changed
- Ignore XAPIAN_* environment variables
- INFO signal showing busy threads callstacks, when tracebacks are enabled
- All exceptions now contain tracebacks, when tracebacks are enabled
- Hash for resolving cluster node for indexes skipping slashes

## Fixed
- Fix ranges in query
- Fix restoring from an empty request
- Formatting applied to saved dates
