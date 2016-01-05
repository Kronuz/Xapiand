/*
Copyright Rene Rivera 2008-2014
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MSGPACK_PREDEF_COMPILER_LLVM_H
#define MSGPACK_PREDEF_COMPILER_LLVM_H

/* Other compilers that emulate this one need to be detected first. */

#include "clang.h"

#include "../version_number.h"
#include "../make.h"

/*`
[heading `MSGPACK_COMP_LLVM`]

[@http://en.wikipedia.org/wiki/LLVM LLVM] compiler.

[table
    [[__predef_symbol__] [__predef_version__]]

    [[`__llvm__`] [__predef_detection__]]
    ]
 */

#define MSGPACK_COMP_LLVM MSGPACK_VERSION_NUMBER_NOT_AVAILABLE

#if defined(__llvm__)
#   define MSGPACK_COMP_LLVM_DETECTION MSGPACK_VERSION_NUMBER_AVAILABLE
#endif

#ifdef MSGPACK_COMP_LLVM_DETECTION
#   if defined(MSGPACK_PREDEF_DETAIL_COMP_DETECTED)
#       define MSGPACK_COMP_LLVM_EMULATED MSGPACK_COMP_LLVM_DETECTION
#   else
#       undef MSGPACK_COMP_LLVM
#       define MSGPACK_COMP_LLVM MSGPACK_COMP_LLVM_DETECTION
#   endif
#   define MSGPACK_COMP_LLVM_AVAILABLE
#include "../detail/comp_detected.h"
#endif

#define MSGPACK_COMP_LLVM_NAME "LLVM"

#include "../detail/test.h"
MSGPACK_PREDEF_DECLARE_TEST(MSGPACK_COMP_LLVM,MSGPACK_COMP_LLVM_NAME)

#ifdef MSGPACK_COMP_LLVM_EMULATED
#include "../detail/test.h"
MSGPACK_PREDEF_DECLARE_TEST(MSGPACK_COMP_LLVM_EMULATED,MSGPACK_COMP_LLVM_NAME)
#endif


#endif
