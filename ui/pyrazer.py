#!/usr/bin/env python
"""
#   Razer device configuration
#   High level user interface library
#
#   This library connects to the lowlevel 'razerd' system daemon.
#
#   Copyright (C) 2008-2009 Michael Buesch <mb@bu3sch.de>
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version 2
#   of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
"""

import socket
import select
import hashlib

RAZER_VERSION	= "0.07"



class RazerEx(Exception):
	"Exception thrown by pyrazer code."


def razer_be32_to_int(be32Str):
	return ((ord(be32Str[0]) << 24) | \
	        (ord(be32Str[1]) << 16) | \
	        (ord(be32Str[2]) << 8)  | \
	        (ord(be32Str[3])))

def razer_be16_to_int(be16Str):
	return ((ord(be16Str[0]) << 8)  | \
	        (ord(be16Str[1])))

def razer_int_to_be32(integer):
	return "%c%c%c%c" % ((integer >> 24) & 0xFF,\
			     (integer >> 16) & 0xFF,\
			     (integer >> 8) & 0xFF,\
			     (integer & 0xFF))

def razer_int_to_be16(integer):
	return "%c%c" % ((integer >> 8) & 0xFF,\
			 (integer & 0xFF))

class RazerDevId:
	"devid parser"

	DEVTYPE_UNKNOWN = "Unknown"
	DEVTYPE_MOUSE = "Mouse"

	BUSTYPE_UNKNOWN = "Unknown"
	BUSTYPE_USB = "USB"

	def __init__(self, devid):
		self.devtype = self.DEVTYPE_UNKNOWN
		self.bustype = self.BUSTYPE_UNKNOWN
		self.buspos = ""
		self.devname = ""
		self.devid = ""
		try:
			id = devid.split(':')
			self.devtype = id[0]
			self.devname = id[1]
			bus = id[2].split('-')
			self.bustype = bus[0]
			self.buspos = bus[1]
			if len(bus) >= 3:
				self.buspos += "-" + bus[2]
			self.devid = id[3]
		except IndexError:
			pass

	def getDevType(self):
		"Returns DEVTYPE_..."
		return self.devtype
	def getBusType(self):
		"Returns BUSTYPE_..."
		return self.bustype
	def getBusPosition(self):
		"Returns the bus position ID string"
		return self.buspos
	def getDevName(self):
		"Returns the device name string"
		return self.devname
	def getDevId(self):
		"Returns the device ID string"
		return self.devid

class Razer:
	SOCKET_PATH	= "/var/run/razerd/socket"
	PRIVSOCKET_PATH	= "/var/run/razerd/socket.privileged"

	INTERFACE_REVISION = 1

	COMMAND_MAX_SIZE = 512
	COMMAND_HDR_SIZE = 1
	BULK_CHUNK_SIZE = 128
	RAZER_IDSTR_MAX_SIZE = 128
	RAZER_LEDNAME_MAX_SIZE = 64

	COMMAND_ID_GETREV = 0		# Get the revision number of the socket interface.
	COMMAND_ID_RESCANMICE = 1	# Rescan mice.
	COMMAND_ID_GETMICE = 2		# Get a list of detected mice.
	COMMAND_ID_GETFWVER = 3		# Get the firmware rev of a mouse.
	COMMAND_ID_SUPPFREQS = 4	# Get a list of supported frequencies.
	COMMAND_ID_SUPPRESOL = 5	# Get a list of supported resolutions.
	COMMAND_ID_SUPPDPIMAPPINGS = 6	# Get a list of supported DPI mappings.
	COMMAND_ID_CHANGEDPIMAPPING = 7	# Modify a DPI mapping.
	COMMAND_ID_GETDPIMAPPING = 8	# Get the active DPI mapping for a profile.
	COMMAND_ID_SETDPIMAPPING = 9	# Set the active DPI mapping for a profile.
	COMMAND_ID_GETLEDS = 10		# Get a list of LEDs on the device.
	COMMAND_ID_SETLED = 11		# Set the state of a LED.
	COMMAND_ID_GETFREQ = 12		# Get the current frequency.
	COMMAND_ID_SETFREQ = 13		# Set the frequency.
	COMMAND_ID_GETPROFILES = 14	# Get a list of supported profiles.
	COMMAND_ID_GETACTIVEPROF = 15	# Get the active profile.
	COMMAND_ID_SETACTIVEPROF = 16	# Set the active profile.
	COMMAND_ID_SUPPBUTTONS = 17	# Get a list of physical buttons.
	COMMAND_ID_SUPPBUTFUNCS = 18	# Get a list of supported button functions.
	COMMAND_ID_GETBUTFUNC = 19	# Get the current function of a button.
	COMMAND_ID_SETBUTFUNC = 20	# Set the current function of a button.

	COMMAND_PRIV_FLASHFW = 128	# Upload and flash a firmware image
	COMMAND_PRIV_CLAIM = 129	# Claim the device.
	COMMAND_PRIV_RELEASE = 130	# Release the device.

	# Replies to commands
	REPLY_ID_U32 = 0		# An unsigned 32bit integer.
	REPLY_ID_STR = 1		# A string
	# Notifications. These go through the reply channel.
	__NOTIFY_ID_FIRST = 128
	NOTIFY_ID_NEWMOUSE = 128	# New mouse was connected.
	NOTIFY_ID_DELMOUSE = 129	# A mouse was removed.


	def __init__(self, enableNotifications=False):
		"Connect to razerd."
		self.enableNotifications = enableNotifications
		self.notifications = []
		try:
			self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.sock.connect(self.SOCKET_PATH)
		except socket.error, e:
			raise RazerEx("Failed to connect to razerd socket: %s" % e)
		try:
			self.privsock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.privsock.connect(self.PRIVSOCKET_PATH)
		except socket.error, e:
			self.privsock = None # No privileged access

		self.__sendCommand(self.COMMAND_ID_GETREV)
		rev = self.__recvU32()
		if (rev != self.INTERFACE_REVISION):
			raise RazerEx("Incompatible interface revision. razerd=%u, me=%u" %\
					(rev, self.INTERFACE_REVISION))

	def __constructCommand(self, commandId, idstr, payload):
		cmd = "%c" % commandId
		idstr += '\0' * (self.RAZER_IDSTR_MAX_SIZE - len(idstr))
		cmd += idstr
		cmd += payload
		cmd += '\0' * (self.COMMAND_MAX_SIZE - len(cmd))
		return cmd

	def __send(self, data):
		self.sock.sendall(data)

	def __sendPrivileged(self, data):
		try:
			self.privsock.sendall(data)
		except (socket.error, AttributeError):
			raise RazerEx("Privileged command failed. Do you have permission?")

	def __sendBulkPrivileged(self, data):
		for i in range(0, len(data), self.BULK_CHUNK_SIZE):
			chunk = data[i : i + self.BULK_CHUNK_SIZE]
			self.__sendPrivileged(chunk)
			result = self.__recvU32Privileged()
			if result != 0:
				raise RazerEx("Privileged bulk write failed. %u" % result)

	def __sendCommand(self, commandId, idstr="", payload=""):
		cmd = self.__constructCommand(commandId, idstr, payload)
		self.__send(cmd)

	def __sendPrivilegedCommand(self, commandId, idstr="", payload=""):
		cmd = self.__constructCommand(commandId, idstr, payload)
		self.__sendPrivileged(cmd)

	def __handleReceivedMessage(self, packet):
		id = packet[0]
		if id < self.__NOTIFY_ID_FIRST:
			raise RazerEx("Received unhandled packet %u" % id)
		if self.enableNotifications:
			self.notifications.append(packet)

	def __receive(self, sock):
		"Receive the next message. This will block until a message arrives."
		hdrlen = 1
		hdr = sock.recv(hdrlen)
		id = ord(hdr[0])
		payload = None
		if id == self.REPLY_ID_U32:
			payload = razer_be32_to_int(sock.recv(4))
		elif id == self.REPLY_ID_STR:
			strlen = razer_be16_to_int(sock.recv(2))
			payload = sock.recv(strlen)
		elif id == self.NOTIFY_ID_NEWMOUSE:
			pass
		elif id == self.NOTIFY_ID_DELMOUSE:
			pass
		else:
			raise RazerEx("Received unknown message (id=%u)" % id)

		return (id, payload)

	def __receiveExpectedMessage(self, sock, expectedId):
		"""Receive messages until the expected one appears.
		Unexpected messages will be handled by __handleReceivedMessage.
		This function returns the payload of the expected message."""
		while 1:
			pack = self.__receive(sock)
			if (pack[0] == expectedId):
				break
			else:
				self.__handleReceivedMessage(pack)
		return pack[1]

	def __recvU32(self):
		"Receive an expected REPLY_ID_U32"
		return self.__receiveExpectedMessage(self.sock, self.REPLY_ID_U32)

	def __recvU32Privileged(self):
		"Receive an expected REPLY_ID_U32 on the privileged socket"
		try:
			return self.__receiveExpectedMessage(self.privsock, self.REPLY_ID_U32)
		except (socket.error, AttributeError):
			raise RazerEx("Privileged recvU32 failed. Do you have permission?")

	def __recvString(self):
		"Receive an expected REPLY_ID_STR"
		return self.__receiveExpectedMessage(self.sock, self.REPLY_ID_STR)

	def pollNotifications(self):
		"Returns a list of pending notifications (id, payload)"
		if not self.enableNotifications:
			raise RazerEx("Polled notifications while notifications were disabled")
		while 1:
			res = select.select([self.sock], [], [], 0.001)
			if not res[0]:
				break
			pack = self.__receive(self.sock)
			self.__handleReceivedMessage(pack)
		notifications = self.notifications
		self.notifications = []
		return notifications

	def rescanMice(self):
		"Send the command to rescan for mice to the daemon."
		self.__sendCommand(self.COMMAND_ID_RESCANMICE)

	def getMice(self):
		"Returns a list of ID-strings for the detected mice."
		self.__sendCommand(self.COMMAND_ID_GETMICE)
		count = self.__recvU32()
		mice = []
		for i in range(0, count):
			mice.append(self.__recvString())
		return mice

	def getFwVer(self, idstr):
		"Returns the firmware version. The returned value is a tuple (major, minor)."
		self.__sendCommand(self.COMMAND_ID_GETFWVER, idstr)
		rawVer = self.__recvU32()
		return ((rawVer >> 8) & 0xFF, rawVer & 0xFF)

	def getSupportedFreqs(self, idstr):
		"Returns a list of supported frequencies for a mouse."
		self.__sendCommand(self.COMMAND_ID_SUPPFREQS, idstr)
		count = self.__recvU32()
		freqs = []
		for i in range(0, count):
			freqs.append(self.__recvU32())
		return freqs

	def getCurrentFreq(self, idstr, profileId):
		"Returns the currently selected frequency for a mouse."
		payload = razer_int_to_be32(profileId)
		self.__sendCommand(self.COMMAND_ID_GETFREQ, idstr, payload)
		return self.__recvU32()

	def getSupportedRes(self, idstr):
		"Returns a list of supported resolutions for a mouse."
		self.__sendCommand(self.COMMAND_ID_SUPPRESOL, idstr)
		count = self.__recvU32()
		res = []
		for i in range(0, count):
			res.append(self.__recvU32())
		return res

	def getLeds(self, idstr):
		"Returns a list of LEDs on the mouse. Each entry is a tuple (name, state)."
		self.__sendCommand(self.COMMAND_ID_GETLEDS, idstr)
		count = self.__recvU32()
		leds = []
		for i in range(0, count):
			name = self.__recvString()
			state = self.__recvU32()
			leds.append( (name, state) )
		return leds

	def setLed(self, idstr, ledName, newState):
		"Set a LED to a new state."
		ledName = ledName.strip()
		if len(ledName) > self.RAZER_LEDNAME_MAX_SIZE:
			raise RazerEx("LED name string too long")
		payload = ledName
		payload += '\0' * (self.RAZER_LEDNAME_MAX_SIZE - len(ledName))
		if newState:
			payload += "%c" % 1
		else:
			payload += "%c" % 0
		self.__sendCommand(self.COMMAND_ID_SETLED, idstr, payload)
		return self.__recvU32()

	def setFrequency(self, idstr, profileId, newFrequency):
		"Set a new scan frequency (in Hz)."
		payload = razer_int_to_be32(profileId) + razer_int_to_be32(newFrequency)
		self.__sendCommand(self.COMMAND_ID_SETFREQ, idstr, payload)
		return self.__recvU32()

	def getSupportedDpiMappings(self, idstr):
		"Returns a list of supported DPI mappings. Each entry is a tuple (id, resolution, isMutable)"
		self.__sendCommand(self.COMMAND_ID_SUPPDPIMAPPINGS, idstr)
		count = self.__recvU32()
		mappings = []
		for i in range(0, count):
			id = self.__recvU32()
			res = self.__recvU32()
			mutable = self.__recvU32()
			mappings.append( (id, res, mutable) )
		return mappings

	def changeDpiMapping(self, idstr, mappingId, newResolution):
		"Changes the resolution value of a DPI mapping."
		payload = razer_int_to_be32(mappingId) + razer_int_to_be32(newResolution)
		self.__sendCommand(self.COMMAND_ID_CHANGEDPIMAPPING, idstr, payload)
		return self.__recvU32()

	def getDpiMapping(self, idstr, profileId):
		"Gets the resolution mapping of a profile."
		payload = razer_int_to_be32(profileId)
		self.__sendCommand(self.COMMAND_ID_GETDPIMAPPING, idstr, payload)
		return self.__recvU32()

	def setDpiMapping(self, idstr, profileId, mappingId):
		"Sets the resolution mapping of a profile."
		payload = razer_int_to_be32(profileId) + razer_int_to_be32(mappingId)
		self.__sendCommand(self.COMMAND_ID_SETDPIMAPPING, idstr, payload)
		return self.__recvU32()

	def getProfiles(self, idstr):
		"Returns a list of profiles. Each entry is the profile ID."
		self.__sendCommand(self.COMMAND_ID_GETPROFILES, idstr)
		count = self.__recvU32()
		profiles = []
		for i in range(0, count):
			profiles.append(self.__recvU32())
		return profiles

	def getActiveProfile(self, idstr):
		"Returns the ID of the active profile."
		self.__sendCommand(self.COMMAND_ID_GETACTIVEPROF, idstr)
		return self.__recvU32()

	def setActiveProfile(self, idstr, profileId):
		"Selects the active profile."
		payload = razer_int_to_be32(profileId)
		self.__sendCommand(self.COMMAND_ID_SETACTIVEPROF, idstr, payload)
		return self.__recvU32()

	def flashFirmware(self, idstr, image):
		"Flash a new firmware on the device. Needs high privileges!"
		payload = razer_int_to_be32(len(image))
		self.__sendPrivilegedCommand(self.COMMAND_PRIV_FLASHFW, idstr, payload)
		self.__sendBulkPrivileged(image)
		return self.__recvU32Privileged()

	def getSupportedButtons(self, idstr):
		"Get a list of supported buttons. Each entry is a tuple (id, name)."
		self.__sendCommand(self.COMMAND_ID_SUPPBUTTONS, idstr)
		buttons = []
		count = self.__recvU32()
		for i in range(0, count):
			id = self.__recvU32()
			name = self.__recvString()
			buttons.append( (id, name) )
		return buttons

	def getSupportedButtonFunctions(self, idstr):
		"Get a list of possible button functions. Each entry is a tuple (id, name)."
		self.__sendCommand(self.COMMAND_ID_SUPPBUTFUNCS, idstr)
		funcs = []
		count = self.__recvU32()
		for i in range(0, count):
			id = self.__recvU32()
			name = self.__recvString()
			funcs.append( (id, name) )
		return funcs

	def getButtonFunction(self, idstr, profileId, buttonId):
		"Get a button function. Returns a tuple (id, name)."
		payload = razer_int_to_be32(profileId) + razer_int_to_be32(buttonId)
		self.__sendCommand(self.COMMAND_ID_GETBUTFUNC, idstr, payload)
		id = self.__recvU32()
		name = self.__recvString()
		return (id, name)

	def setButtonFunction(self, idstr, profileId, buttonId, functionId):
		"Set a button function."
		payload = razer_int_to_be32(profileId) +\
			  razer_int_to_be32(buttonId) +\
			  razer_int_to_be32(functionId)
		self.__sendCommand(self.COMMAND_ID_SETBUTFUNC, idstr, payload)
		return self.__recvU32()

class RazerConfigFile:
	DEFAULT_PATH = "/etc/razer.conf"

	def __init__(self, path = DEFAULT_PATH):
		self.path = path

	def save(self, razer):
		"Save the data of a 'class Razer' instance to the config file."
		#TODO

	def load(self, razer):
		"Load the data from the config file to the specified 'class Razer'"
		#TODO

class IHEXParser:
	TYPE_DATA = 0
	TYPE_EOF  = 1
	TYPE_ESAR = 2
	TYPE_SSAR = 3
	TYPE_ELAR = 4
	TYPE_SLAR = 5

	def __init__(self, ihex):
		self.ihex = ihex

	def parse(self):
		bin = []
		try:
			lines = self.ihex.splitlines()
			hiAddr = 0
			for line in lines:
				line = line.strip()
				if len(line) == 0:
					continue
				if len(line) < 11 or (len(line) - 1) % 2 != 0:
					raise RazerEx("Invalid firmware file format (IHEX length error)")
				if line[0] != ':':
					raise RazerEx("Invalid firmware file format (IHEX magic error)")
				count = int(line[1:3], 16)
				if len(line) != count * 2 + 11:
					raise RazerEx("Invalid firmware file format (IHEX count error)")
				addr = (int(line[3:5], 16) << 8) | int(line[5:7], 16)
				addr |= hiAddr << 16
				type = int(line[7:9], 16)
				checksum = 0
				for i in range(1, len(line), 2):
					byte = int(line[i:i+2], 16)
					checksum = (checksum + byte) & 0xFF
				checksum = checksum & 0xFF
				if checksum != 0:
					raise RazerEx("Invalid firmware file format (IHEX checksum error)")

				if type == self.TYPE_EOF:
					break
				if type == self.TYPE_ELAR:
					if count != 2:
						raise RazerEx("Invalid firmware file format (IHEX inval ELAR)")
					hiAddr = (int(line[9:11], 16) << 8) | int(line[11:13], 16)
					continue
				if type == self.TYPE_DATA:
					if len(bin) < addr + count: # Reallocate
						bin += ['\0'] * (addr + count - len(bin))
					for i in range(9, 9 + count * 2, 2):
						byte = chr(int(line[i:i+2], 16))
						if bin[(i - 9) / 2 + addr] != '\0':
							raise RazerEx("Invalid firmware file format (IHEX corruption)")
						bin[(i - 9) / 2 + addr] = byte
					continue
				raise RazerEx("Invalid firmware file format (IHEX unsup type %d)" % type)
		except ValueError:
			raise RazerEx("Invalid firmware file format (IHEX digit format)")
		return "".join(bin)

class SRECParser:
	def __init__(self, srec):
		self.srec = srec

	def parse(self):
		return ""#TODO

class RazerFirmwareParser:
	class Descriptor:
		def __init__(self, startOffset, endOffset, parser, binTruncate):
			# startOffset: The offset where the ihex/srec/etc starts
			# endOffset: The offset where the ihex/srec/etc ends
			# parser: ihex/srec/etc parser
			# binTruncate: Number of bytes to truncate the binary to
			self.start = startOffset
			self.len = endOffset - startOffset + 1
			self.parser = parser
			self.binTruncate = binTruncate

	DUMP = 0 # Set to 1 to dump all images to /tmp

	FWLIST = {
		# Deathadder 1.27
		"92d7f44637858405a83c0f192c61388c" : Descriptor(0x14B28, 0x1D8F4, IHEXParser, 0x4000)
	}

	def __init__(self, filepath):
		try:
			self.data = file(filepath, "rb").read()
		except IOError, e:
			raise RazerEx("Could not read file: %s" % e.strerror)
		md5sum = hashlib.md5(self.data).hexdigest().lower()
		try:
			descriptor = self.FWLIST[md5sum]
		except KeyError:
			raise RazerEx("Unsupported firmware file")
		try:
			rawFwData = self.data[descriptor.start : descriptor.start+descriptor.len]
			if self.DUMP:
				file("/tmp/razer.dump", "wb").write(rawFwData)
			fwImage = descriptor.parser(rawFwData).parse()
			if self.DUMP:
				file("/tmp/razer.dump.image", "wb").write(fwImage)
			if descriptor.binTruncate:
				fwImage = fwImage[:descriptor.binTruncate]
			if self.DUMP:
				file("/tmp/razer.dump.image.trunc", "wb").write(fwImage)
		except IndexError:
			raise RazerEx("Invalid firmware file format")
		self.fwImage = fwImage

	def getImage(self):
		return self.fwImage
