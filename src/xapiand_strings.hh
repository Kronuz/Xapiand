/*
 * The string utilities now live in the standalone library
 * github.com/Kronuz/strings. This is a thin Xapiand wrapper, named
 * xapiand_strings.hh (not strings.hh) so it never shadows the library's strings.hh
 * on the include path -- extracted deps compiled into Xapiand (e.g. traceback.cc)
 * do a bare #include "strings.hh" and must reach the real library header, not this
 * wrapper (which pulls in log.h). Because src/ no longer holds a strings.hh, the
 * wrapper reaches the real header with a plain #include "strings.hh" (no absolute-path
 * macro), then re-adds log.h, which Xapiand files have long obtained transitively
 * through strings.hh (the extracted library correctly no longer pulls it in).
 */

#pragma once

#include "strings.hh"     // the extracted strings.hh (the real API)
#include "log.h"          // historically reached transitively via strings.hh
