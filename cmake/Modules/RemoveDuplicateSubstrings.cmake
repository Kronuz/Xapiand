# - Adds a compiler flag if it is supported by the compiler
#
# This function checks that the supplied compiler flag is supported and then
# adds it to the corresponding compiler flags
#
#  add_cxx_compiler_flag(<FLAG> [<VARIANT>])
#
# - Example
#
# include (AddCXXCompilerFlag)
# remove_duplicate_substrings("a a a b b c" OUTPUT)
# OUTPUT will now be "a b c"

if (__remove_duplicate_substrings)
	return()
endif ()

set (__remove_duplicate_substrings INCLUDED)

function (remove_duplicate_substrings stringIn stringOut)
    separate_arguments(stringIn)
    list(REMOVE_DUPLICATES stringIn)
    string(REPLACE ";" " " stringIn "${stringIn}")
    set (${stringOut} "${stringIn}" PARENT_SCOPE)
endfunction ()
