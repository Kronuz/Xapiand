/*
 * The string utilities now live in the standalone library
 * github.com/Kronuz/strings. This is a thin Xapiand wrapper: it pulls the real
 * header in by absolute path (STRINGS_HEADER, set in CMakeLists.txt, to dodge the
 * same-name "strings.hh" collision the way src/colors.h and src/namegen.h do),
 * then re-adds log.h, which Xapiand files have long obtained transitively through
 * strings.hh (the extracted library correctly no longer pulls it in).
 */

#pragma once

#include STRINGS_HEADER   // the extracted strings.hh (the real API)
#include "log.h"          // historically reached transitively via strings.hh
