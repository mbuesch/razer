#!/usr/bin/env python

from distutils.core import setup, Extension

pyrazer = Extension("pyrazer",
#		    define_macros	= [("foobar", "1")],
		    include_dirs	= ["../librazer"],
		    libraries		= ["razer", "usb"],
		    library_dirs	= ["../librazer"],
                    sources		= ["pyrazer.c"])

setup(name		= "pyrazer",
      version		= "0.1",
      description	= "Razer device driver library",
      author		= "Michael Buesch",
      author_email	= "mb@bu3sch.de",
#     url		= 'http://...',
      ext_modules	= [pyrazer])
