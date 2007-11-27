add_library(razer SHARED
	    librazer.c
	    hw_deathadder.c)

INSTALL(FILES librazer.h DESTINATION include)
INSTALL(TARGETS razer DESTINATION lib)

