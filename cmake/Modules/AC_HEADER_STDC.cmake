# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

message(STATUS "Checking whether system has ANSI C header files")

include (CheckIncludeFiles)
include (CheckPrototypeExists)

check_include_files("dlfcn.h;stdint.h;stddef.h;inttypes.h;stdlib.h;strings.h;string.h;float.h" StandardHeadersExist)
if (StandardHeadersExist)
	check_prototype_exists(memchr string.h memchrExists)
	if (memchrExists)

		check_prototype_exists(free stdlib.h freeExists)
		if (freeExists)
			message(STATUS "ANSI C header files - found")
			set (STDC_HEADERS 1 CACHE INTERNAL "System has ANSI C header files")
			set (HAVE_STRINGS_H 1)
			set (HAVE_STRING_H 1)
			set (HAVE_FLOAT_H 1)
			set (HAVE_STDLIB_H 1)
			set (HAVE_STDDEF_H 1)
			set (HAVE_STDINT_H 1)
			set (HAVE_INTTYPES_H 1)
			set (HAVE_DLFCN_H 1)
		endif (freeExists)
	endif (memchrExists)
endif (StandardHeadersExist)

if (NOT STDC_HEADERS)
	message(STATUS "ANSI C header files - not found")
	set (STDC_HEADERS 0 CACHE INTERNAL "System has ANSI C header files")
endif (NOT STDC_HEADERS)

check_include_files(unistd.h HAVE_UNISTD_H)

include (CheckCSourceCompiles)
check_c_source_compiles("#include <sys/stat.h>\n#include <sys/types.h>\n#include <dirent.h>\nint main(void) { if ((DIR *) 0) return 0; return 0; }" HAVE_DIRENT_H)
if (HAVE_DIRENT_H)
	set (HAVE_SYS_STAT_H 1)
	set (HAVE_SYS_TYPES_H 1)
endif (HAVE_DIRENT_H)

check_include_files("dlfcn.h;stdint.h;stddef.h;inttypes.h;stdlib.h;strings.h;string.h;float.h" StandardHeadersExist)
set (HAVE_LIBM 1)
