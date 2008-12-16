add_executable(razerd
	       razerd.c)

set_target_properties(razerd PROPERTIES
		      COMPILE_FLAGS "-std=c99 -Wall -pedantic")

add_definitions("-Du_int8_t=uint8_t -Du_int16_t=uint16_t -Du_int32_t=uint32_t")
add_definitions("-D_GNU_SOURCE")

include_directories("${razer_SOURCE_DIR}/librazer")

target_link_libraries(razerd razer)
