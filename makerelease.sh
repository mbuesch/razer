#!/bin/bash
set -e

project="razercfg"


origin="$(pwd)"
version="$(cat $origin/ui/pyrazer.py | grep -e RAZER_VERSION | head -n1 | cut -d'"' -f2)"
if [ -z "$version" ]; then
	echo "Could not determine version!"
	exit 1
fi
release_name="$project-$version"
tarball="$release_name.tar.bz2"

export GIT_DIR="$origin/.git"

cd /tmp/
rm -Rf "$release_name" "$tarball"
echo "Creating target directory"
mkdir "$release_name"
cd "$release_name"
echo "git checkout"
git checkout -f

rm makerelease.sh .gitignore hacking.py

echo "creating tarball"
cd ..
tar cjf "$tarball" "$release_name"
mv "$tarball" "$origin"

echo "running testbuild"
cd "$release_name"
cmake .
make

echo "removing testbuild"
cd ..
rm -R "$release_name"

echo
echo "built release $version"
