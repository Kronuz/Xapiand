/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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
