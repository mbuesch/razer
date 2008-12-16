project(razer C)

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

add_subdirectory(librazer)
add_subdirectory(razerd)
#add_subdirectory(pyrazer)
add_subdirectory(razercfg)
add_subdirectory(qrazercfg)

