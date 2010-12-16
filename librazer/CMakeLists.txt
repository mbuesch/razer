include("${razer_SOURCE_DIR}/cmake.global")

add_library(razer SHARED
	    librazer.c
	    cypress_bootloader.c
	    hw_copperhead.c
	    hw_deathadder.c
	    hw_krait.c
	    hw_lachesis.c
	    hw_naga.c)

set_target_properties(razer PROPERTIES
		      COMPILE_FLAGS "-std=c99 -Wall")

add_definitions("-Du_int8_t=uint8_t -Du_int16_t=uint16_t -Du_int32_t=uint32_t")
add_definitions("-D_GNU_SOURCE")

target_link_libraries(razer usb)

install(TARGETS razer DESTINATION lib)


add_custom_target(razer_udev_rules
		   COMMAND ${CMAKE_SOURCE_DIR}/gen_udev_rules.sh
		   ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_INSTALL_PREFIX}
		   DEPENDS ${CMAKE_SOURCE_DIR}/01-razer-udev.rules.template
		   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		   COMMENT "Generate udev rules")
add_dependencies(razer razer_udev_rules)

install(FILES ${CMAKE_BINARY_DIR}/01-razer-udev.rules
	DESTINATION /etc/udev/rules.d/)

install(CODE "
	execute_process(COMMAND udevadm control --reload-rules RESULT_VARIABLE udevadm_res)
	if (NOT \"\${udevadm_res}\" STREQUAL \"0\")
		message(WARNING \"WARNING: Failed to reload udev rules: \${udevadm_res}\")
		message(WARNING \"WARNING: Please reload udev manually or reboot the system\")
	else (NOT \"\${udevadm_res}\" STREQUAL \"0\")
		message(STATUS \"udevadm control --reload-rules: \${udevadm_res}\")
	endif (NOT \"\${udevadm_res}\" STREQUAL \"0\")
")

install(CODE "
	execute_process(COMMAND ldconfig RESULT_VARIABLE ldconfig_res)
	if (NOT \"\${ldconfig_res}\" STREQUAL \"0\")
		message(WARNING \"WARNING: Failed to run ldconfig: \${ldconfig_res}\")
		message(WARNING \"WARNING: Your system will probably be unable to find librazer.so library!\")
	else (NOT \"\${ldconfig_res}\" STREQUAL \"0\")
		message(STATUS \"ldconfig result: \${ldconfig_res}\")
	endif (NOT \"\${ldconfig_res}\" STREQUAL \"0\")
")
