project(razer C)

cmake_minimum_required(VERSION 2.4)

if (COMMAND cmake_policy)
	# Libraries linked via full path no longer produce linker search paths.
	cmake_policy(SET CMP0003 NEW)
	# Preprocessor definition values are not escaped automatically.
	cmake_policy(SET CMP0005 OLD)
endif (COMMAND cmake_policy)

include("${razer_SOURCE_DIR}/scripts/cmake.global")
include(CheckIncludeFile)

macro(CHECK_LIB _LIB _HEADER)
	check_include_file(${_HEADER} ${_HEADER}_ERR)
	if (NOT ${_HEADER}_ERR)
		message(FATAL_ERROR "Could not find library \"lib${_LIB}\" "
			"with header ${_HEADER}")
	endif (NOT ${_HEADER}_ERR)
endmacro(CHECK_LIB)

check_lib(usb-1.0 libusb.h)

add_subdirectory(librazer)

configure_file("udev.rules.template" "udev.rules")

install(FILES ${CMAKE_BINARY_DIR}/udev.rules
	DESTINATION /etc/udev/rules.d/
	RENAME 01-razer-udev.rules
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)


configure_file("pm-hook.sh.template" "pm-hook.sh" @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/pm-hook.sh
	DESTINATION /etc/pm/sleep.d/
	RENAME 50-razer
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		    GROUP_READ GROUP_EXECUTE
		    WORLD_READ WORLD_EXECUTE)


configure_file("razerd.service.template" "razerd.service")

install(FILES ${CMAKE_BINARY_DIR}/razerd.service
	DESTINATION /etc/systemd/system/
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)

install_exec_cmd("udevadm control --reload-rules"
		 "Please reload udev manually or reboot the system")

install_exec_cmd("systemctl --system daemon-reload"
		 "If you use systemd, please reload systemd manually or reboot the system")

add_subdirectory(razerd)
add_subdirectory(ui)
