# Based on official CMake's FindGTest.cmake

function (FindTools_append_debugs _endvar _library)
	if (${_library} AND ${_library}_DEBUG)
		set (_output optimized ${${_library}} debug ${${_library}_DEBUG})
	else ()
		set (_output ${${_library}})
	endif ()
	set (${_endvar} ${_output} PARENT_SCOPE)
endfunction ()


function (FindTools_find_include _name _root)
	find_path(${_name}
		NAMES ${ARGN}
		HINTS
			"${${_root}}"
			"$ENV{${_root}}"
		PATH_SUFFIXES include
	)
	mark_as_advanced("${_name}")
endfunction ()


function (FindTools_find_library _name _root)
	find_library(${_name}
		NAMES ${ARGN}
		HINTS
			"${${_root}}"
			"$ENV{${_root}}"
		PATH_SUFFIXES lib
	)
	mark_as_advanced("${_name}")
endfunction ()


macro (FindTools_determine_windows_library_type _var)
	if (EXISTS "${${_var}}")
		file(TO_NATIVE_PATH "${${_var}}" _lib_path)
		get_filename_component(_name "${${_var}}" NAME_WE)
		file(STRINGS "${${_var}}" _match REGEX "${_name}\\.dll" LIMIT_COUNT 1)
		if (NOT _match STREQUAL "")
			set (${_var}_TYPE SHARED PARENT_SCOPE)
		else ()
			set (${_var}_TYPE UNKNOWN PARENT_SCOPE)
		endif ()
	endif ()
endmacro ()


function (FindTools_determine_library_type _var)
	if (WIN32)
		# For now, at least, only Windows really needs to know the library type
		FindTools_determine_windows_library_type(${_var})
		FindTools_determine_windows_library_type(${_var}_RELEASE)
		FindTools_determine_windows_library_type(${_var}_DEBUG)
	endif ()
	# If we get here, no determination was made from the above checks
	set (${_var}_TYPE UNKNOWN PARENT_SCOPE)
endfunction ()


function (FindTools_import_library _target _var _config)
	if (_config)
		set (_config_suffix "_${_config}")
	else ()
		set (_config_suffix "")
	endif ()

	set (_lib "${${_var}${_config_suffix}}")
	if (EXISTS "${_lib}")
		if (_config)
			set_property(TARGET ${_target} APPEND PROPERTY
				IMPORTED_CONFIGURATIONS ${_config})
		endif ()
		set_target_properties(${_target} PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES${_config_suffix} "CXX")
		if (WIN32 AND ${_var}_TYPE STREQUAL SHARED)
			set_target_properties(${_target} PROPERTIES
				IMPORTED_IMPLIB${_config_suffix} "${_lib}")
		else ()
			set_target_properties(${_target} PROPERTIES
				IMPORTED_LOCATION${_config_suffix} "${_lib}")
		endif ()
	endif ()
endfunction ()


function (FindTools_add_library _name _var)
    FindTools_determine_library_type(${_var}_LIBRARY)
    add_library(${_name} ${${_var}_LIBRARY_TYPE} IMPORTED)
    if (${_var}_LIBRARY_TYPE STREQUAL "SHARED")
        set_target_properties(${_name} PROPERTIES
            INTERFACE_COMPILE_DEFINITIONS "${_var}_LINKED_AS_SHARED_LIBRARY=1")
    endif ()
    if (${_var}_INCLUDE_DIRS)
        set_target_properties(${_name} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${${_var}_INCLUDE_DIRS}")
    endif ()
    FindTools_import_library(${_name} ${_var} "")
    FindTools_import_library(${_name} ${_var} "RELEASE")
    FindTools_import_library(${_name} ${_var} "DEBUG")
endfunction ()
