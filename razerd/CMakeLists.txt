include(${razer_SOURCE_DIR}/scripts/cmake.global)

add_executable(razerd
	       razerd.c)

set_target_properties(razerd PROPERTIES COMPILE_FLAGS ${GENERIC_COMPILE_FLAGS})

include_directories("${razer_SOURCE_DIR}/librazer")

target_link_libraries(razerd razer)
install(TARGETS razerd DESTINATION bin)
