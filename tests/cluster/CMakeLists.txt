# The cluster's unit tests (src/cluster). Standalone until the top-level CMakeLists.txt pins
# consensus (journal B1): then it adds this directory and the targets below find consensus there.
#   cmake -S tests/cluster -B build-cluster -DFETCHCONTENT_BASE_DIR="$PWD/.deps-cluster" && cmake --build build-cluster && ctest --test-dir build-cluster
cmake_minimum_required(VERSION 3.16)
project(xapiand_cluster_tests LANGUAGES CXX)

set(XAPIAND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(FetchContent)
if(NOT TARGET consensus::transport)
	if(APPLE AND NOT OPENSSL_ROOT_DIR)
		execute_process(COMMAND brew --prefix openssl@3 OUTPUT_VARIABLE OPENSSL_ROOT_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	endif()
	# consensus's own pins for task and completion, as its standalone build makes them.
	FetchContent_Declare(task GIT_REPOSITORY https://github.com/Kronuz/task.git GIT_TAG 95798a02f68c86f070401329a05c78d326ac2579)
	FetchContent_Declare(completion GIT_REPOSITORY https://github.com/Kronuz/completion.git GIT_TAG d8b538c6778f0a1b969e42c8428995d1d29d4731)
	set(TASK_ASIO ON)
	set(COMPLETION_TASK ON)
	set(COMPLETION_ASIO ON)
	FetchContent_MakeAvailable(task completion)
	set(CONSENSUS_TRANSPORT ON)
	FetchContent_Declare(consensus GIT_REPOSITORY https://github.com/Kronuz/consensus.git GIT_TAG e1f249d8d9ff64410f87042ba90f4c218d9ec336)  # consensus main: the RPC lane
	FetchContent_MakeAvailable(consensus)
endif()
if(NOT TARGET msgpack::msgpack)
	FetchContent_Declare(msgpack GIT_REPOSITORY https://github.com/Kronuz/msgpack.git GIT_TAG 1e9aaaf4ac5bfefa97c51d6f5884c1f3b61f80a5)  # Xapiand's pin
	FetchContent_MakeAvailable(msgpack)
endif()

enable_testing()
add_library(xapiand_cluster_rpc STATIC "${XAPIAND_ROOT}/src/cluster/rpc.cc" "${XAPIAND_ROOT}/src/cluster/rpc_methods.cc")
target_include_directories(xapiand_cluster_rpc PUBLIC "${XAPIAND_ROOT}/src")
target_compile_features(xapiand_cluster_rpc PUBLIC cxx_std_20)
target_link_libraries(xapiand_cluster_rpc PUBLIC consensus::transport msgpack::msgpack)

foreach(t rpc rpc_methods)
	add_executable(xapiand_cluster_test_${t} test_${t}.cc)
	target_include_directories(xapiand_cluster_test_${t} PRIVATE "${XAPIAND_ROOT}/tests" "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(xapiand_cluster_test_${t} PRIVATE xapiand_cluster_rpc)
endforeach()
foreach(c unary streams statuses handler-throws versions deadline cancel lane-faults framing)
	add_test(NAME cluster_rpc_${c} COMMAND xapiand_cluster_test_rpc ${c})
	set_tests_properties(cluster_rpc_${c} PROPERTIES TIMEOUT 120)
endforeach()
foreach(c codec malformed wire)
	add_test(NAME cluster_rpc_methods_${c} COMMAND xapiand_cluster_test_rpc_methods ${c})
	set_tests_properties(cluster_rpc_methods_${c} PROPERTIES TIMEOUT 120)
endforeach()
