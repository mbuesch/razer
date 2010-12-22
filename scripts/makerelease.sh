#!/bin/bash
set -e

project="razercfg"

basedir="$(dirname "$0")"
[ "${basedir:0:1}" = "/" ] || basedir="$PWD/$basedir"

origin="$basedir/.."

version="$(cat $origin/ui/pyrazer.py | grep -e RAZER_VERSION | head -n1 | cut -d'"' -f2)"
if [ -z "$version" ]; then
	echo "Could not determine version!"
	exit 1
fi
release_name="$project-$version"
tarball="$release_name.tar.bz2"
tagname="release-$version"
tagmsg="$project-$version release"

export GIT_DIR="$origin/.git"

cd /tmp/
rm -Rf "$release_name" "$tarball"
echo "Creating target directory"
mkdir "$release_name"
cd "$release_name"
echo "git checkout"
git checkout -f

rm scripts/makerelease.sh .gitignore
rm -R firmware #XXX Remove it for now...

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


echo "Tagging GIT"
cd "$origin"
git tag -m "$tagmsg" -a "$tagname"

echo
echo "built release $version"
