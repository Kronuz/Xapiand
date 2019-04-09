---
title: "Xapiand 0.19.2 Released"
date: "2019-04-09 15:00:00 -0600"
author: Kronuz
version: 0.19.2
categories: [release]
---

Colorized INFO (ctrl-t) for easyly detecting problematic states + fix libev issue.


### Changed
- Colorized INFO
- Tracebacks with normalized address
- Require CMake 3.12

### Fixed
- Use nanosleep instead of usleep and libev checking for EINTR
- Fix warning about available files under OS X
