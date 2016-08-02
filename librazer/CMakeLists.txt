include("${razer_SOURCE_DIR}/scripts/cmake.global")

add_library(razer SHARED
	    buttonmapping.c
	    profile_emulation.c
	    librazer.c
	    config.c
	    util.c
	    synapse.c
	    cypress_bootloader.c
	    hw_boomslangce.c
	    hw_copperhead.c
	    hw_deathadder.c
	    hw_deathadder2013.c
	    hw_deathadder_chroma.c
	    hw_krait.c
	    hw_lachesis.c
	    hw_lachesis5k6.c
	    hw_naga.c
	    hw_imperator.c
	    hw_taipan.c
	    hw_mamba_tournament_edition.c)

set_target_properties(razer PROPERTIES COMPILE_FLAGS ${GENERIC_COMPILE_FLAGS}
				       SOVERSION 1)

add_definitions("-Du_int8_t=uint8_t -Du_int16_t=uint16_t -Du_int32_t=uint32_t")

target_link_libraries(razer usb-1.0)

install(TARGETS razer DESTINATION lib)

install_exec_cmd("ldconfig"
		 "Your system will probably be unable to locate librazer.so library")
