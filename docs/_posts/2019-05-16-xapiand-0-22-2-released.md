---
title: "Xapiand 0.22.2 Released"
date: "2019-05-16 23:09:00 -0600"
author: Kronuz
version: 0.22.2
categories: [release]
---

This release fixes some "undefined behavior" issues as well as fixes an
issue which prevented detecting invalid datatypes passed in '_type' during
indexation of objects.


### Added
- Support for 'float' as an alternative name to 'floating' datatype

### Fixed
- Undefined behavior in UUID, serializer and term generator
- Error when requesting invalid types
