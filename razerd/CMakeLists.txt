include(${razer_SOURCE_DIR}/cmake.global)

add_executable(razerd
	       razerd.c)

set_target_properties(razerd PROPERTIES
		      COMPILE_FLAGS "-std=gnu99 -Wall")

include_directories("${razer_SOURCE_DIR}/librazer")

target_link_libraries(razerd razer)
