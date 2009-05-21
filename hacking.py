#!/usr/bin/env python
#
# Template script to probe lowlevel accesses on hardware.
# Might be useful for reverse engineering.
#

import sys
import usb


# XXX: Set the USB ID of your device here
vendor = 0x1532
product = 0x0007


def doStuff(dev):
	h = dev.open()
	h.claimInterface(dev.configurations[0].interfaces[0][0].interfaceNumber)

	try:
		# XXX: Add the operations here
		pass
	except usb.USBError, e:
		print e

	h.releaseInterface()

for bus in usb.busses():
	for dev in bus.devices:
		if dev.idProduct == product and dev.idVendor == vendor:
			doStuff(dev)
			break
