include("${razer_SOURCE_DIR}/scripts/cmake.global")


set(PYRAZER_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/pyrazer/__init__.py"
		 "${CMAKE_CURRENT_SOURCE_DIR}/pyrazer/main.py")

set(PYRAZER_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/build")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in"
	       "${CMAKE_CURRENT_SOURCE_DIR}/setup.py")

add_custom_command(OUTPUT ${PYRAZER_BUILD}/stamp
		   COMMAND ${PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/setup.py" build -b "${PYRAZER_BUILD}"
		   COMMAND ${CMAKE_COMMAND} -E touch ${PYRAZER_BUILD}/stamp
		   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		   DEPENDS ${PYRAZER_DEPS})

add_custom_target(pyrazer ALL
		  DEPENDS ${PYRAZER_BUILD}/stamp)

install(CODE
	"execute_process(COMMAND ${PYTHON} \"${CMAKE_CURRENT_SOURCE_DIR}/setup.py\"
			           install --prefix \"${CMAKE_INSTALL_PREFIX}\"
		         WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}\"
			 RESULT_VARIABLE pyrazer_install_res)
	if (NOT \"\${pyrazer_install_res}\" STREQUAL \"0\")
		message(FATAL_ERROR \"Failed to install pyrazer\")
	endif()
")

install(PROGRAMS razer-gamewrapper
	DESTINATION bin)
