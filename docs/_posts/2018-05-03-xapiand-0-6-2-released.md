---
title: "Xapiand 0.6.2 Released"
date: "2018-05-03 09:00:00 -0600"
author: Kronuz
version: 0.6.2
categories: [release]
---

This release fixes a problem where restore command blew memory usage. Documents
are now spoon-fed to the index, preventing that atrocity.

Commands `MERGE` and `PATCH` now check document exists or return 404.

The HTTP server failed with 500 when receiving spaces or malformed messages,
this is no longer the case, it now properly returns the HTTP error code and
version. It also returned incomplete bodies when objects where large.

Indexing now correctly guesses ID, also using information passed by user as a
hint for that purpose.

When passing integers (and some other uncommon types) as IDs, it failed
to add new documents with a message saying something like integer cannot be
converted to UUID.

Date accuracies where some times returning errors when used.

A long standing bug which assumed read() on files always returned the
full requested size when available.

### Added
- Added `--flush-threshold` command line option

### Changed
- `MERGE` and `PATCH` now require the document already exists
- Renamed command line option `--force-up` to simply `--force`

### Fixed
- Fix restore command memory usage
- Fix HTTP protocol on malformed messages
- Fix HTTP returning of large object bodies
- Fix indexing documents with integer IDs
- Fix io::read() to always return requested size when available
- Fix date accuracies
