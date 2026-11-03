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
 * Trace hooks for the `scheduler` library.
 *
 * Self-contained no-op stubs. The previous version included Xapiand's log.h to
 * "restore" colored scheduler traces, but the extracted logger's macros expand to
 * Logging::do_log (needs the full Logging class) and the scheduler is a dependency
 * of the logger, so pulling log.h here created a circular include.
 *
 * scheduler.h self-guards and cleans up L_SCHEDULER (and only mentions L_PRINT /
 * L_BLUE in a comment or inside no-op trace args), so those need not be defined
 * here. It does use L_DEBUG_HOOK / L_EXC / L_CALL, which we stub to no-ops (their
 * default; off in Xapiand by default anyway). L_EXC is later #undef'd and made
 * real by the logger for Xapiand's own call sites.
 */

#pragma once

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif
#ifndef L_DEBUG_HOOK
#define L_DEBUG_HOOK L_NOTHING
#endif
#ifndef L_EXC
#define L_EXC L_NOTHING
#endif
#ifndef L_CALL
#define L_CALL L_NOTHING
#endif
