/* generator_stubs.c: link stubs for the snowball compiler's non-C backends.
 *
 * The vendored snowball compiler (driver.c) has a switch over every target
 * language it can emit -- Ada, C#, Dart, Go, Java, JavaScript, Pascal, PHP,
 * Python, Rust -- calling generate_program_<lang>() for each.  In snowball
 * 1.5.0 those calls were wrapped in `#ifndef DISABLE_<LANG>` and our build
 * defines those macros so only the C backend (generator.c) is compiled.
 * Snowball 2.0.0 dropped the guards (and added the Ada/Dart/PHP backends),
 * so driver.c now references all ten backends unconditionally, leaving the
 * `snowball` tool with undefined symbols since we vendor only generator.c.
 *
 * Xapiand only ever generates C/C++ stemmers (snowball is always invoked with
 * `-c++`, i.e. LANG_C), so none of these backends is ever selected.  Rather
 * than vendoring ~10 unused code generators, we provide empty stubs so the
 * driver's switch links.  They are dead code for our build; if one were ever
 * reached it would simply emit nothing.  Only compiled into the `snowball`
 * host tool (compiler/*.c), never into the Xapian library.
 */

#include "header.h"

#define XAPIAN_SNOWBALL_STUB(lang) \
    void generate_program_##lang(struct generator * g) { (void)g; }

XAPIAN_SNOWBALL_STUB(java)
XAPIAN_SNOWBALL_STUB(csharp)
XAPIAN_SNOWBALL_STUB(pascal)
XAPIAN_SNOWBALL_STUB(python)
XAPIAN_SNOWBALL_STUB(js)
XAPIAN_SNOWBALL_STUB(rust)
XAPIAN_SNOWBALL_STUB(go)
XAPIAN_SNOWBALL_STUB(ada)
XAPIAN_SNOWBALL_STUB(dart)
XAPIAN_SNOWBALL_STUB(php)
