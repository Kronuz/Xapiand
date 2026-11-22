# The cluster's unit tests (src/cluster) and the RPC decoders' fuzz target, in the top-level build
# under BUILD_TESTS (CMakeLists.txt adds this directory when TLS is on: consensus's transport needs
# OpenSSL 3).

# The cluster code builds against msgpack as the library ships it: its own exception seam and rapidjson flags.
get_directory_property(cluster_definitions COMPILE_DEFINITIONS)
list(FILTER cluster_definitions EXCLUDE REGEX "^(MSGPACK_EXCEPTION_HEADER|RAPIDJSON_PARSE_DEFAULT_FLAGS)=")
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${cluster_definitions}")

set(CLUSTER_SRC "${PROJECT_SOURCE_DIR}/src/cluster")

add_library(xapiand_cluster_rpc STATIC "${CLUSTER_SRC}/rpc.cc" "${CLUSTER_SRC}/rpc_methods.cc")
target_include_directories(xapiand_cluster_rpc PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(xapiand_cluster_rpc PUBLIC cxx_std_20)
target_link_libraries(xapiand_cluster_rpc PUBLIC consensus::transport msgpack::msgpack)

foreach(t rpc rpc_methods)
	add_executable(xapiand_cluster_test_${t} test_${t}.cc)
	target_include_directories(xapiand_cluster_test_${t} PRIVATE "${PROJECT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(xapiand_cluster_test_${t} PRIVATE xapiand_cluster_rpc)
	add_dependencies(check xapiand_cluster_test_${t})
endforeach()
foreach(c unary streams statuses handler-throws versions deadline cancel lane-faults framing)
	add_test(NAME cluster_rpc_${c} COMMAND xapiand_cluster_test_rpc ${c})
	set_tests_properties(cluster_rpc_${c} PROPERTIES TIMEOUT 120)
endforeach()
foreach(c codec malformed wire)
	add_test(NAME cluster_rpc_methods_${c} COMMAND xapiand_cluster_test_rpc_methods ${c})
	set_tests_properties(cluster_rpc_methods_${c} PROPERTIES TIMEOUT 120)
endforeach()

# Membership's admission rule and keepalive table: no transport, no msgpack.
add_library(xapiand_cluster_membership STATIC "${CLUSTER_SRC}/membership_admission.cc" "${CLUSTER_SRC}/membership_liveness.cc")
target_include_directories(xapiand_cluster_membership PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(xapiand_cluster_membership PUBLIC cxx_std_20)
add_executable(xapiand_cluster_test_membership test_membership.cc)
target_include_directories(xapiand_cluster_test_membership PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(xapiand_cluster_test_membership PRIVATE xapiand_cluster_membership)
add_dependencies(check xapiand_cluster_test_membership)
foreach(c admission histories cold liveness duplicate)
	add_test(NAME cluster_membership_${c} COMMAND xapiand_cluster_test_membership ${c})
	set_tests_properties(cluster_membership_${c} PROPERTIES TIMEOUT 120)
endforeach()

# The seed corpus: one valid envelope per decoder (fuzz_rpc_seed DIR).
add_executable(fuzz_rpc_seed fuzz_rpc_seed.cc)
target_include_directories(fuzz_rpc_seed PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fuzz_rpc_seed PRIVATE xapiand_cluster_rpc)

# libFuzzer target, built with -DFUZZ=ON like the others: it compiles the code it fuzzes itself.
if (FUZZ)
	add_executable(fuzz_rpc_methods fuzz_rpc_methods.cc "${CLUSTER_SRC}/rpc_methods.cc" "${CLUSTER_SRC}/rpc.cc")
	target_include_directories(fuzz_rpc_methods PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
	target_compile_features(fuzz_rpc_methods PRIVATE cxx_std_20)
	target_link_libraries(fuzz_rpc_methods PRIVATE consensus::transport msgpack::msgpack)
	target_compile_options(fuzz_rpc_methods PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
	target_link_options(fuzz_rpc_methods PRIVATE -fsanitize=fuzzer,address,undefined)
endif ()
