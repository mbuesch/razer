project(razer C)

cmake_minimum_required(VERSION 2.4)

if (COMMAND cmake_policy)
	# Libraries linked via full path no longer produce linker search paths.
	cmake_policy(SET CMP0003 NEW)
	# Preprocessor definition values are not escaped automatically.
	cmake_policy(SET CMP0005 OLD)
endif (COMMAND cmake_policy)

include(CheckIncludeFile)

macro(CHECK_LIB _LIB _HEADER)
	check_include_file(${_HEADER} ${_HEADER}_ERR)
	if (NOT ${_HEADER}_ERR)
		message(FATAL_ERROR "Could not find library \"lib${_LIB}\". "
			"On Debian Linux try \"apt-get install lib${_LIB}-dev\" "
			"to install the library.")
	endif (NOT ${_HEADER}_ERR)
endmacro(CHECK_LIB)

check_lib(usb usb.h)

set(CMAKE_C_FLAGS_DEBUG "-DDEBUG ${CMAKE_C_FLAGS_DEBUG}")

add_subdirectory(librazer)
add_subdirectory(razerd)
add_subdirectory(ui)
