#!/bin/sh

srcdir="$(realpath "$0" | xargs dirname)"
srcdir="$srcdir/.."

# Import the makerelease.lib
# https://bues.ch/cgit/misc.git/tree/makerelease.lib
die() { echo "$*"; exit 1; }
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
