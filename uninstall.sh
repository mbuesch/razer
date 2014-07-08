#!/bin/sh
#
#   razercfg uninstaller
#
#   Copyright (C) 2014 Michael Buesch <m@bues.ch>
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


die()
{
	echo "$*"
	exit 1
}

# $1=message
ask()
{
	read -p "$*? [Y/n]" ok
	[ "$ok" = "y" -o "$ok" = "Y" -o \
	  "$ok" = "1" -o "$ok" = "" ] && return 0
	return 1
}

# $1=file/dir
uninstall()
{
	local path="$1"

	ask "Delete $path" || return
	rm -r "$path" || die "Failed to delete '$path'"
}

# $1=prefix
uninstall_prefix()
{
	local prefix="$1"

	for f in /bin/pyrazer.py /bin/pyrazer.pyc /bin/pyrazer.pyo\
		 /bin/razercfg /bin/qrazercfg /bin/razer-gamewrapper\
		 /sbin/razerd /bin/razerd; do

		local path="${prefix}${f}"
		[ -e "$path" ] || continue
		uninstall "$path"
	done

	for f in "$prefix"/lib/python*/site-packages/pyrazer; do

		local path="$f"
		[ -e "$path" ] || continue
		uninstall "$path"
	done
}

uninstall_global()
{
	for f in /etc/razer.conf /etc/init.d/razerd /etc/rc*.d/*razerd\
		 /etc/pm/sleep.d/*-razer /etc/udev/rules.d/*-razer-udev.rules\
		 /{etc,usr/lib}/systemd/system/razerd.service; do

		local path="$f"
		[ -e "$path" ] || continue
		uninstall "$path"
	done
}

help()
{
	echo "Usage: uninstall.sh PREFIX"
	echo
	echo "PREFIX is the prefix where razercfg was installed to."
	echo "This usually is /usr/local or /usr"
	echo
	echo "So an example uninstall call might look like this:"
	echo "  ./uninstall.sh /usr/local"
}

[ $# -eq 1 ] || {
	help
	exit 1
}
PREFIX="$1"

uninstall_prefix "$PREFIX"
uninstall_global
exit 0
