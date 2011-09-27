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
		  COMMAND ${CMAKE_SOURCE_DIR}/scripts/paths_replace.sh
			  ${CMAKE_SOURCE_DIR}/udev.rules.template
			  ${CMAKE_BINARY_DIR}/udev.rules
			  ${CMAKE_INSTALL_PREFIX}
		  DEPENDS ${CMAKE_SOURCE_DIR}/udev.rules.template
		  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		  COMMENT "Generate udev rules")
add_dependencies(razer razer_udev_rules)

install(FILES ${CMAKE_BINARY_DIR}/udev.rules
	DESTINATION /etc/udev/rules.d/
	RENAME 01-razer-udev.rules
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)


add_custom_target(razer_pm_hook
		  COMMAND ${CMAKE_SOURCE_DIR}/scripts/paths_replace.sh
			  ${CMAKE_SOURCE_DIR}/pm-hook.sh.template
			  ${CMAKE_BINARY_DIR}/pm-hook.sh
			  ${CMAKE_INSTALL_PREFIX}
		  DEPENDS ${CMAKE_SOURCE_DIR}/pm-hook.sh.template
		  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		  COMMENT "Generate PM hook")
add_dependencies(razer razer_pm_hook)

install(FILES ${CMAKE_BINARY_DIR}/pm-hook.sh
	DESTINATION /etc/pm/sleep.d/
	RENAME 50-razer
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		    GROUP_READ GROUP_EXECUTE
		    WORLD_READ WORLD_EXECUTE)


install_exec_cmd("udevadm control --reload-rules"
		 "Please reload udev manually or reboot the system")

install_exec_cmd("ldconfig"
		 "Your system will probably be unable to locate librazer.so library")
