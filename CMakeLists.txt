cmake_minimum_required(VERSION 3.5)
project(razer C)

if (COMMAND cmake_policy)
	# Libraries linked via full path no longer produce linker search paths.
	cmake_policy(SET CMP0003 NEW)
	# Preprocessor definition values shall be escaped automatically.
	cmake_policy(SET CMP0005 NEW)
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
	DESTINATION /etc/udev/rules.d
	RENAME 80-razer.rules)

configure_file("pm-hook.sh.template" "pm-hook.sh" @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/pm-hook.sh
	DESTINATION /etc/pm/sleep.d
	RENAME 50-razer
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		    GROUP_READ GROUP_EXECUTE
		    WORLD_READ WORLD_EXECUTE)

configure_file("razerd.service.template" "razerd.service")

install(FILES ${CMAKE_BINARY_DIR}/razerd.service
	DESTINATION /usr/lib/systemd/system)
	
install(FILES ${CMAKE_SOURCE_DIR}/tmpfile.conf
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/tmpfiles.d
	RENAME razerd.conf)

add_subdirectory(razerd)
add_subdirectory(ui)
