#!/bin/sh

srcdir="$(dirname "$0")"
[ "$(echo "$srcdir" | cut -c1)" = '/' ] || srcdir="$PWD/$srcdir"

srcdir="$srcdir/.."

die() { echo "$*"; exit 1; }

# Import the makerelease.lib
# http://bues.ch/gitweb?p=misc.git;a=blob_plain;f=makerelease.lib;hb=HEAD
for path in $(echo "$PATH" | tr ':' ' '); do
	[ -f "$MAKERELEASE_LIB" ] && break
	MAKERELEASE_LIB="$path/makerelease.lib"
done
[ -f "$MAKERELEASE_LIB" ] && . "$MAKERELEASE_LIB" || die "makerelease.lib not found."

hook_get_version()
{
	local file="$1/ui/pyrazer/main.py"
	version="$(cat "$file" | grep -e RAZER_VERSION | head -n1 | cut -d'"' -f2)"
}

hook_post_checkout()
{
	default_hook_post_checkout "$@"

	# Remove firmware directory from release.
	rm -r "$1/firmware"
}

project=razercfg
makerelease "$@"
