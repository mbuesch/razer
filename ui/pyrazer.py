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

	def __init__(self):
		self.__connect()

	def __connect(self):
		"Connect to razerd."
		try:
			self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
			self.sock.connect(self.SOCKET_PATH)
		except socket.error, e:
			raise RazerEx("Failed to connect to razerd: %s" % e.message)
