cmake_minimum_required(VERSION 2.8.10)
project(gbenchmark_builder C CXX)

include (ExternalProject)

ExternalProject_Add(
	googlebenchmark
	PREFIX          "googlebenchmark"
	GIT_REPOSITORY  "https://github.com/google/benchmark.git"
	GIT_TAG         "master"

	# Never update automatically from the remote repository
	UPDATE_DISCONNECTED ON

	# All necessary targets will be built on install stage
	INSTALL_COMMAND ""

	# Wrap steps in a script to log output
	LOG_DOWNLOAD ON
	LOG_UPDATE ON
	LOG_CONFIGURE ON
	LOG_BUILD ON
	LOG_TEST ON
	LOG_INSTALL ON

	# Add flags for configure.
	CMAKE_ARGS
		"-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
		"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}"
		"-DCMAKE_CXX_FLAGS_${UPPER_CMAKE_BUILD_TYPE}:STRING=${CMAKE_CXX_FLAGS_${UPPER_CMAKE_BUILD_TYPE}}"
		"-DBENCHMARK_ENABLE_TESTING:BOOL=OFF"
)

# Specify include dir
ExternalProject_Get_Property(googlebenchmark source_dir)
set (GBENCHMARK_INCLUDE_DIR ${source_dir}/include PARENT_SCOPE)

# Specify MainTest's link libraries
ExternalProject_Get_Property(googlebenchmark binary_dir)
set (GBENCHMARK_LIBRARY_DIR ${binary_dir}/src PARENT_SCOPE)
