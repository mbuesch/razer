include(FindPythonLibs)

include_directories(${PYTHON_INCLUDE_PATH})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../librazer)

macro(ADD_PYTHON_MODULE _NAME _STATIC_SRCS)
	option(MODULE_${_NAME}_ENABLE "Build Python module ${_NAME}" TRUE)

	if (MODULE_${_NAME}_ENABLE)
		add_library(${_NAME} MODULE ${ARGN})
		# Remove the "lib" prefix
		set_target_properties(${_NAME} PROPERTIES PREFIX "")
	endif (MODULE_${_NAME}_ENABLE)
endmacro (ADD_PYTHON_MODULE)

add_python_module(pyrazer MOD_SRCS pyrazer.c)
target_link_libraries(pyrazer razer usb)

#TODO install
