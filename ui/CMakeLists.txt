include("${razer_SOURCE_DIR}/scripts/cmake.global")

install(PROGRAMS pyrazer.py DESTINATION bin)
install(PROGRAMS razercfg qrazercfg razer-gamewrapper DESTINATION bin)
