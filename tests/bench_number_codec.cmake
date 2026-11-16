# The number codec micro-benchmark (design T8). Included by CMakeLists.txt; a job appends it to the old
# codec's tree, so both builds time the same source with the same flags.
add_executable(${PROJECT_NAME}_bench_number_codec
	"${PROJECT_SOURCE_DIR}/tests/bench_number_codec.cc"
	"${PROJECT_SOURCE_DIR}/tests/test_stubs.cc"
)
target_link_libraries(${PROJECT_NAME}_bench_number_codec PRIVATE
	XAPIAND_PACKAGE_OBJ
	XAPIAND_OBJ
	BOOLEAN_PARSER_OBJ
	LZ4_OBJ
	prometheus-cpp::core
	YAML_OBJ
	$<TARGET_NAME_IF_EXISTS:LUA_OBJ>
	${CMAKE_THREAD_LIBS_INIT}
	${UUID_LIBRARIES}
	${M_LIBRARIES}
	${DL_LIBRARIES}
	${ZLIB_LIBRARIES}
)
