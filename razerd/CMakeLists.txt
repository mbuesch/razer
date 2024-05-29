include(${razer_SOURCE_DIR}/scripts/cmake.global)

add_executable(razerd
	       razerd.c)

set_target_properties(razerd PROPERTIES COMPILE_FLAGS ${GENERIC_COMPILE_FLAGS})

include_directories("${razer_SOURCE_DIR}/librazer")

target_link_libraries(razerd razer)
install(TARGETS razerd DESTINATION bin)

if (NOT DEFINED ENV{RPM_BUILD_ROOT} AND NOT DEFINED ENV{RAZERCFG_PKG_BUILD})
	install_exec_cmd("systemctl --system daemon-reload"
			 "If you use systemd, please reload systemd manually or reboot the system")
	install_exec_cmd("systemctl --system --force enable razerd.service"
			 "If you use systemd, enable razerd.service manually")
	install_exec_cmd("systemctl --system start razerd.service"
			 "If you use systemd, start razerd.service manually")
	install_exec_cmd("udevadm control --reload-rules"
			 "Please reload udev manually or reboot the system")
endif (NOT DEFINED ENV{RPM_BUILD_ROOT} AND NOT DEFINED ENV{RAZERCFG_PKG_BUILD})
