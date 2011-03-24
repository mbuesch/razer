include("${razer_SOURCE_DIR}/scripts/cmake.global")

add_library(razer SHARED
	    buttonmapping.c
	    profile_emulation.c
	    librazer.c
	    config.c
	    util.c
	    cypress_bootloader.c
	    hw_boomslangce.c
	    hw_copperhead.c
	    hw_deathadder.c
	    hw_krait.c
	    hw_lachesis.c
	    hw_naga.c)

set_target_properties(razer PROPERTIES COMPILE_FLAGS ${GENERIC_COMPILE_FLAGS})

add_definitions("-Du_int8_t=uint8_t -Du_int16_t=uint16_t -Du_int32_t=uint32_t")

target_link_libraries(razer usb-1.0)

install(TARGETS razer DESTINATION lib)


add_custom_target(razer_udev_rules
		   COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen_udev_rules.sh
		   ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_INSTALL_PREFIX}
		   DEPENDS ${CMAKE_SOURCE_DIR}/01-razer-udev.rules.template
		   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		   COMMENT "Generate udev rules")
add_dependencies(razer razer_udev_rules)

install(FILES ${CMAKE_BINARY_DIR}/01-razer-udev.rules
	DESTINATION /etc/udev/rules.d/)

install_exec_cmd("udevadm control --reload-rules"
		 "Please reload udev manually or reboot the system")

install_exec_cmd("ldconfig"
		 "Your system will probably be unable to locate librazer.so library")
