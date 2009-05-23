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

RAZER_VERSION	= "0.02"



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

class RazerEx(Exception):
	"Exception thrown by pyrazer code."

class Razer:
	SOCKET_PATH	= "/var/run/razerd/socket"
	PRIVSOCKET_PATH	= "/var/run/razerd/socket.privileged"

	INTERFACE_REVISION = 0

	COMMAND_MAX_SIZE = 512
	COMMAND_HDR_SIZE = 1
	BULK_CHUNK_SIZE = 128
	RAZER_IDSTR_MAX_SIZE = 128
	RAZER_LEDNAME_MAX_SIZE = 64

	COMMAND_ID_GETREV = 0		# Get the revision number of the socket interface.
	COMMAND_ID_RESCANMICE = 1	# Rescan mice
	COMMAND_ID_GETMICE = 2		# Get a list of detected mice.
	COMMAND_ID_GETFWVER = 3		# Get the firmware rev of a mouse.
	COMMAND_ID_SUPPFREQS = 4	# Get a list of supported frequencies.
	COMMAND_ID_SUPPRESOL = 5	# Get a list of supported resolutions.
	COMMAND_ID_GETLEDS = 6		# Get a list of LEDs on the device.
	COMMAND_ID_SETLED = 7		# Set the state of a LED.
	COMMAND_ID_SETRES = 8		# Set the resolution.
	COMMAND_ID_SETFREQ = 9		# Set the frequency.
	COMMAND_ID_GETRES = 10		# Get the current resolution.
	COMMAND_ID_GETFREQ = 11		# Get the current frequency.

	COMMAND_PRIV_FLASHFW = 128	# Upload and flash a firmware image

	# Replies to commands
	REPLY_ID_U32 = 0		# An unsigned 32bit integer.
	REPLY_ID_STR = 1		# A string
	# Notifications. These go through the reply channel.
	NOTIFY_ID_NEWMOUSE = 128	# New mouse was connected.
	NOTIFY_ID_DELMOUSE = 129	# A mouse was removed.


	def __init__(self):
		"Connect to razerd."
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
				raise RazerEx("Privileged bulk write failed.")

	def __sendCommand(self, commandId, idstr="", payload=""):
		cmd = self.__constructCommand(commandId, idstr, payload)
		self.__send(cmd)

	def __sendPrivilegedCommand(self, commandId, idstr="", payload=""):
		cmd = self.__constructCommand(commandId, idstr, payload)
		self.__sendPrivileged(cmd)

	def __handleReceivedMessage(self, packet):
		print "got notification", packet[0]
		pass#TODO

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
	
	def getCurrentFreq(self, idstr):
		"Returns the currently selected frequency for a mouse."
		self.__sendCommand(self.COMMAND_ID_GETFREQ, idstr)
		return self.__recvU32()

	def getSupportedRes(self, idstr):
		"Returns a list of supported resolutions for a mouse."
		self.__sendCommand(self.COMMAND_ID_SUPPRESOL, idstr)
		count = self.__recvU32()
		res = []
		for i in range(0, count):
			res.append(self.__recvU32())
		return res

	def getCurrentRes(self, idstr):
		"Returns the currently selected resolution for a mouse."
		self.__sendCommand(self.COMMAND_ID_GETRES, idstr)
		return self.__recvU32()

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

	def setFrequency(self, idstr, newFrequency):
		"Set a new scan frequency (in Hz)."
		payload = razer_int_to_be32(newFrequency)
		self.__sendCommand(self.COMMAND_ID_SETFREQ, idstr, payload)
		return self.__recvU32()

	def setResolution(self, idstr, newResolution):
		"Set a new scan resolution (in DPI)."
		payload = razer_int_to_be32(newResolution)
		self.__sendCommand(self.COMMAND_ID_SETRES, idstr, payload)
		return self.__recvU32()

	def flashFirmware(self, idstr, image):
		"Flash a new firmware on the device. Needs high privileges!"
		payload = razer_int_to_be32(len(image))
		self.__sendPrivilegedCommand(self.COMMAND_PRIV_FLASHFW, idstr, payload)
		self.__sendBulkPrivileged(image)
		return self.__recvU32Privileged()
