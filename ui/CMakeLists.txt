include("${razer_SOURCE_DIR}/scripts/cmake.global")


set(PYRAZER_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/pyrazer/__init__.py"
		 "${CMAKE_CURRENT_SOURCE_DIR}/pyrazer/main.py"
		 "${CMAKE_CURRENT_SOURCE_DIR}/pyrazer/ui.py")

set(PYRAZER_BUILD "${CMAKE_CURRENT_BINARY_DIR}/build")


configure_file(setup.py.in setup.py)

add_custom_command(OUTPUT ${PYRAZER_BUILD}/stamp
		   COMMAND ${PYTHON} "${CMAKE_CURRENT_BINARY_DIR}/setup.py" build --build-base "${PYRAZER_BUILD}"
		   COMMAND ${CMAKE_COMMAND} -E touch ${PYRAZER_BUILD}/stamp
		   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		   DEPENDS ${PYRAZER_DEPS})

add_custom_target(pyrazer ALL
		  DEPENDS ${PYRAZER_BUILD}/stamp)

install(CODE
	"set(PYDESTDIR \"\$ENV{DESTDIR}\")
	if (\"\${PYDESTDIR}\" STREQUAL \"\")
		set(PYDESTDIR /)
	endif()
	execute_process(COMMAND ${PYTHON} \"${CMAKE_CURRENT_BINARY_DIR}/setup.py\"
			build --build-base \"${PYRAZER_BUILD}\" install --root \"\${PYDESTDIR}\" --prefix \"${CMAKE_INSTALL_PREFIX}\"
		        WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}\"
			RESULT_VARIABLE pyrazer_install_res)
	if (NOT \"\${pyrazer_install_res}\" STREQUAL \"0\")
		message(FATAL_ERROR \"Failed to install pyrazer\")
	endif()
")

install(PROGRAMS razer-gamewrapper
	DESTINATION bin)

configure_file(razercfg.desktop.template razercfg.desktop)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/razercfg.desktop
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications/
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/razercfg-icon.svg
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps/
	RENAME razercfg.svg
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/razercfg-alt.svg
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps/
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/razercfg-snake.svg
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps/
	PERMISSIONS OWNER_READ OWNER_WRITE
		    GROUP_READ
		    WORLD_READ)
