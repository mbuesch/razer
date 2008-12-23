include("${razer_SOURCE_DIR}/cmake.global")

add_library(razer SHARED
	    librazer.c
	    hw_deathadder.c
	    hw_krait.c
	    hw_lachesis.c)

set_target_properties(razer PROPERTIES
		      COMPILE_FLAGS "-std=c99 -Wall -pedantic")

add_definitions("-Du_int8_t=uint8_t -Du_int16_t=uint16_t -Du_int32_t=uint32_t")
add_definitions("-D_GNU_SOURCE")

target_link_libraries(razer usb)

install(TARGETS razer DESTINATION lib)
