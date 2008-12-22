#!/usr/bin/env python
"""
#   Razer device configuration
#   High level user interface library
#
#   This library connects to the lowlevel 'razerd' system daemon.
#
#   Copyright (C) 2008 Michael Buesch <mb@bu3sch.de>
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


class RazerEx(Exception):
	"Exception thrown by pyrazer code."

class Razer:
	SOCKET_PATH = "/var/run/razerd/socket"

	INTERFACE_REVISION = 0

	COMMAND_MAX_SIZE = 512
	COMMAND_HDR_SIZE = 1
	RAZER_IDSTR_MAX_SIZE = 128

	COMMAND_ID_GETREV = 0		# Get the revision number of the socket interface.
	COMMAND_ID_GETMICE = 1		# Get a list of detected mice.
	COMMAND_ID_GETFWVER = 2		# Get the firmware rev of a mouse.
	COMMAND_ID_SUPPFREQS = 3	# Get a list of supported frequencies.
	COMMAND_ID_SUPPRESOL = 4	# Get a list of supported resolutions.

	def __init__(self):
		self.__connect()

	def __connect(self):
		"Connect to razerd."
		try:
			self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.sock.connect(self.SOCKET_PATH)
		except socket.error, e:
			raise RazerEx("Failed to connect to razerd socket: %s" % e)

	def __be32_to_int(self, be32Str):
		return ((ord(be32Str[0]) << 24) | \
		        (ord(be32Str[1]) << 16) | \
		        (ord(be32Str[2]) << 8)  | \
		        (ord(be32Str[3])))

	def __doSendCommand(self, rawCommand):
		self.sock.sendall(rawCommand)

	def __sendCommand(self, commandId, idstr="", payload=""):
		cmd = "%c" % commandId
		idstr += '\0' * (self.RAZER_IDSTR_MAX_SIZE - len(idstr))
		cmd += idstr
		cmd += payload
		cmd += '\0' * (self.COMMAND_MAX_SIZE - len(cmd))
		self.__doSendCommand(cmd)

	def __recvU32(self):
		data = self.sock.recv(4)
		return self.__be32_to_int(data)

	def __recvString(self):
		string = ""
		while 1:
			char = self.sock.recv(1)
			if char == '\0':
				break
			string += char
		return string

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

	def getSupportedRes(self, idstr):
		"Returns a list of supported resolutions for a mouse."
		self.__sendCommand(self.COMMAND_ID_SUPPRESOL, idstr)
		count = self.__recvU32()
		res = []
		for i in range(0, count):
			res.append(self.__recvU32())
		return res

