#!/usr/bin/env python

import sys
from shutil import *
from dircache import *

bases = ( "/", "/usr", "/usr/local", "/opt" )
instfiles = ( "pyrazer.so", )


def usage():
	print "Usage: %s MOD_SRC_DIR" % sys.argv[0]

try:
	srcdir = sys.argv[1]
except IndexError:
	usage()
	sys.exit(1)

pyver = sys.version.split()[0] # pyver == "X.X.X"
pyver = pyver.split(".")
major = pyver[0]
minor = pyver[1]
pydir = "python%s.%s" % (major, minor)
modpath = "/lib/" + pydir + "/site-packages"

for base in bases:
	try:
		if not pydir in listdir(base + "/lib"):
			continue
		full_modpath = base + modpath
		# Probe whether it exists
		listdir(full_modpath)
	except OSError:
		continue
	print "Python module path found in " + full_modpath
	try:
		for f in instfiles:
			copy(srcdir + "/" + f, full_modpath)
			print "Installed \"%s\"" % f
	except IOError, e:
		print "ERROR: Could not install module \"%s\"" % f
		print e
		sys.exit(1)
	sys.exit(0)


print "ERROR: Python module install path not found."
print "Python modules are usually found in /usr/lib/pythonX.X/site-packages"
sys.exit(1)
