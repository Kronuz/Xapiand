cmake_minimum_required (VERSION 2.8.8)
project (gtest_builder C CXX)

include (ExternalProject)

ExternalProject_Add(
	googletest
	PREFIX "googletest"
	GIT_REPOSITORY https://github.com/google/googletest.git
	INSTALL_COMMAND ""
)

set (source_dir ${CMAKE_CURRENT_BINARY_DIR}/googletest/src/googletest)
set (binary_dir ${CMAKE_CURRENT_BINARY_DIR}/googletest/src/googletest-build)

set (GTEST_INCLUDE_DIRS
	${source_dir}/googlemock/include
	${source_dir}/googletest/include
	PARENT_SCOPE)

set (GTEST_LIBS
	${binary_dir}/googlemock/gtest
	${binary_dir}/googlemock
	PARENT_SCOPE)

add_library(gtest STATIC IMPORTED)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(gtest_main STATIC IMPORTED)
set_property(TARGET gtest_main PROPERTY IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(gmock STATIC IMPORTED)
set_property(TARGET gmock PROPERTY IMPORTED_LOCATION ${binary_dir}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}gmock${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(gmock_main STATIC IMPORTED)
set_property(TARGET gmock_main PROPERTY IMPORTED_LOCATION ${binary_dir}/googlemock/${CMAKE_STATIC_LIBRARY_PREFIX}gmock_main${CMAKE_STATIC_LIBRARY_SUFFIX})
