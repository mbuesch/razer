add_library(razer SHARED
	    librazer.c
	    hw_deathadder.c
	    hw_krait.c)

install(FILES librazer.h DESTINATION include)
install(TARGETS razer DESTINATION lib)

