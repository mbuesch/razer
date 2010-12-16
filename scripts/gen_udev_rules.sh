#!/bin/bash

set -e

srcdir="$1"
bindir="$2"
instdir="$3"

echo "Generating udev rules"
echo "srcdir=$srcdir  bindir=$bindir  instdir=$instdir"

instdir="$(echo $instdir | sed 's/\//\\\//g')"
ex='s/\$\$INSTDIR\$\$/'"$instdir"'/g'

sed $ex $srcdir/01-razer-udev.rules.template > $bindir/01-razer-udev.rules
