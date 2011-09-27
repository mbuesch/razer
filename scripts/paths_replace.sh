#!/bin/sh
set -e

templatefile="$1"
targetfile="$2"
instdir="$3"

echo "pathreplace: template=$templatefile  target=$targetfile  instdir=$instdir"

instdir="$(echo $instdir | sed 's/\//\\\//g')"
ex='s/\$\$INSTDIR\$\$/'"$instdir"'/g'
sed $ex "$templatefile" > "$targetfile"
